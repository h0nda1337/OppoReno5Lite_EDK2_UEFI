#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Create an offline SYSTEM hive copy with visible, persistent crash diagnostics.

Linux host dependencies: libhivex and python-registry. No live registry,
WIM, card, firmware, kernel or driver is modified by this tool.
"""
import argparse
import ctypes
import ctypes.util
import hashlib
import io
import json
import os
from pathlib import Path
import re
import struct
import tempfile


def fingerprint(data):
    return hashlib.sha256(data).hexdigest()


def inspect(data):
    # Use a different implementation from the writer to check every key/value.
    from Registry import Registry

    hive = Registry.Registry(io.BytesIO(data))
    keys, values = set(), {}
    pending = [("", hive.root(), 0)]
    while pending:
        path, node, depth = pending.pop()
        if depth > 64 or len(keys) >= 100000 or path in keys:
            raise ValueError("Hive traversal limit or duplicate key")
        keys.add(path)
        for value in node.values():
            # Preserve spelling: the reader labels an unnamed value "(default)",
            # which is distinct here from a literal named value "(Default)".
            identity = (path, value.name())
            if len(values) >= 250000 or identity in values:
                raise ValueError("Hive value limit or duplicate name")
            values[identity] = (value.value_type(), bytes(value.raw_data()))
        for child in node.subkeys():
            pending.append(((path + "\\" if path else "") + child.name(), child, depth + 1))
    return hive, keys, values


class Hivex:
    class Value(ctypes.Structure):
        _fields_ = [("key", ctypes.c_char_p), ("type", ctypes.c_int),
                    ("length", ctypes.c_size_t), ("data", ctypes.c_void_p)]

    def __init__(self, library):
        self.lib = ctypes.CDLL(library, use_errno=True)
        signatures = {
            "open": ([ctypes.c_char_p, ctypes.c_int], ctypes.c_void_p),
            "close": ([ctypes.c_void_p], ctypes.c_int),
            "root": ([ctypes.c_void_p], ctypes.c_size_t),
            "node_get_child": ([ctypes.c_void_p, ctypes.c_size_t, ctypes.c_char_p], ctypes.c_size_t),
            "node_add_child": ([ctypes.c_void_p, ctypes.c_size_t, ctypes.c_char_p], ctypes.c_size_t),
            "node_set_value": ([ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(self.Value), ctypes.c_int], ctypes.c_int),
            "commit": ([ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int], ctypes.c_int),
        }
        for name, (args, result) in signatures.items():
            function = getattr(self.lib, "hivex_" + name)
            function.argtypes, function.restype = args, result

    def patch(self, source, destination, controls):
        handle = self.lib.hivex_open(os.fsencode(source), 4)  # HIVEX_OPEN_WRITE, no unsafe mode.
        if not handle:
            raise OSError(ctypes.get_errno(), "Cannot open offline hive")
        try:
            root = self.lib.hivex_root(handle)
            if not root:
                raise ValueError("Missing hive root")
            for control in controls:
                node = root
                for name in (control, "Control", "CrashControl"):
                    child = self.lib.hivex_node_get_child(handle, node, name.encode())
                    if not child and name == "CrashControl":
                        child = self.lib.hivex_node_add_child(handle, node, name.encode())
                    if not child:
                        raise ValueError("Missing control-set path")
                    node = child
                for name, value in (("DisplayParameters", 1), ("AutoReboot", 0)):
                    raw = ctypes.create_string_buffer(struct.pack("<I", value))
                    item = self.Value(name.encode(), 4, 4, ctypes.cast(raw, ctypes.c_void_p))
                    if self.lib.hivex_node_set_value(handle, node, ctypes.byref(item), 0) != 0:
                        raise OSError(ctypes.get_errno(), "Cannot set crash diagnostic DWORD")
            if self.lib.hivex_commit(handle, os.fsencode(destination), 0) != 0:
                raise OSError(ctypes.get_errno(), "Cannot write offline hive copy")
        finally:
            self.lib.hivex_close(handle)


def prepare(source, output, library):
    source = Path(source).resolve(strict=True)
    output = Path(output).absolute()
    if not source.is_file() or source.stat().st_size > 64 * 1024 * 1024:
        raise ValueError("Expected an offline SYSTEM hive of at most 64 MiB")
    if output.exists() or output.is_symlink():
        raise FileExistsError("Output must be a new file")
    data = source.read_bytes()
    hive, keys, values = inspect(data)
    controls = sorted(k for k in keys if re.fullmatch(r"ControlSet[0-9]{3}", k))
    if not 1 <= len(controls) <= 32:
        raise ValueError("Missing or excessive SYSTEM control sets")
    for name in ("Current", "Default", "LastKnownGood"):
        value = hive.open("Select").value(name)
        if value.value_type() != 4 or len(value.raw_data()) != 4:
            raise ValueError("Invalid SYSTEM Select DWORD")
        if f"ControlSet{value.value():03d}" not in controls:
            raise ValueError("SYSTEM Select points outside existing control sets")

    expected_keys, expected_values = set(keys), dict(values)
    edits = []
    for control in controls:
        path = control + "\\Control\\CrashControl"
        if control + "\\Control" not in keys:
            raise ValueError("Control set has no Control key")
        expected_keys.add(path)
        for name, value in (("DisplayParameters", 1), ("AutoReboot", 0)):
            identity = (path, name)
            old = values.get(identity)
            if old is not None and (old[0] != 4 or len(old[1]) != 4):
                raise ValueError("Existing crash setting is not a DWORD")
            expected_values[identity] = (4, struct.pack("<I", value))
            edits.append({"key": path, "name": name, "type": "REG_DWORD", "value": value})

    with tempfile.TemporaryDirectory(prefix="winpe-crash-", dir=output.parent) as temporary:
        candidate = Path(temporary) / "SYSTEM"
        Hivex(library).patch(source, candidate, controls)
        changed = candidate.read_bytes()
        _, changed_keys, changed_values = inspect(changed)
        if changed_keys != expected_keys or changed_values != expected_values:
            raise ValueError("Independent reader found changes outside the allowed settings")
        if source.read_bytes() != data:
            raise ValueError("Source hive changed during preparation")
        # Publish only the fully verified copy; never replace an existing output.
        with output.open("xb") as target:
            target.write(changed)
            target.flush()
            os.fsync(target.fileno())
    return {"input_sha256": fingerprint(data), "output_sha256": fingerprint(changed),
            "input_bytes": len(data), "output_bytes": len(changed),
            "keys_checked": len(keys), "values_checked": len(values),
            "all_other_keys_and_value_bytes_unchanged": True,
            "source_unchanged": True, "edits": edits,
            "scope": "offline PC hive copy only; physical behavior unverified",
            "automatic_restart_disabled": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--hivex-library", default=ctypes.util.find_library("hivex"))
    args = parser.parse_args()
    if not args.hivex_library:
        parser.error("libhivex is required")
    print(json.dumps(prepare(args.source, args.output, args.hivex_library), indent=2))


if __name__ == "__main__":
    main()
