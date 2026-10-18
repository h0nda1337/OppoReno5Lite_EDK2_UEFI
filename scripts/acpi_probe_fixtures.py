#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Synthetic MT6779-shaped FDTs for the read-only ACPI profile gate. No dumps."""
from copy import deepcopy
from pathlib import Path
import struct


def words(*values):
    return struct.pack('>'+str(len(values))+'I', *values)


def node(name, props=(), children=()):
    return dict(name=name, props=list(props), children=list(children))


def fixture():
    cpus = [node('cpu@'+str(i), [('device_type', b'cpu\0'), ('reg', words(i << 8)),
            ('compatible', b'arm,cortex-a55\0' if i < 6 else b'arm,cortex-a75\0'),
            ('enable-method', b'psci\0')]) for i in range(8)]
    # Deliberately reverse traversal order and retain misleading node suffixes.
    cpu = node('cpus', [('#address-cells', words(1)), ('#size-cells', words(0))], cpus[::-1])
    gic = node('interrupt-controller@0c000000', [('compatible', b'arm,gic-v3\0'),
        ('reg', words(0,0xc000000,0,0x40000, 0,0xc040000,0,0x200000, 0,0xc53a650,0,0x50)),
        ('interrupts', words(1,9,4)), ('interrupt-controller', b''),
        ('#interrupt-cells', words(3)), ('#redistributor-regions', words(1)), ('phandle', words(2))])
    soc = node('soc', [('compatible', b'simple-bus\0'), ('#address-cells', words(2)),
                     ('#size-cells', words(2)), ('ranges', b'')], [gic])
    return node('', [('compatible', b'mediatek,MT6779\0'), ('#address-cells', words(2)),
        ('#size-cells', words(2))], [cpu, soc,
        node('psci', [('compatible', b'arm,psci-1.0\0'), ('method', b'smc\0')]),
        node('timer', [('compatible', b'arm,armv8-timer\0'), ('interrupt-parent', words(2)),
                       ('interrupts', words(1,13,0xff08, 1,14,0xff08, 1,11,0xff08, 1,10,0xff08))]),
        node('pmu', [('compatible', b'arm,armv8-pmuv3\0'), ('interrupt-parent', words(2)),
                     ('interrupts', words(1,7,8))])])


def get(tree, path):
    for part in path.strip('/').split('/') if path != '/' else []:
        tree = next(x for x in tree['children'] if x['name'] == part)
    return tree


def replace(tree, path, key, value):
    props = get(tree, path)['props']
    props[:] = [(k,v) for k,v in props if k != key] + ([] if value is None else [(key,value)])


def pack(tree):
    block, table, offsets = bytearray(), bytearray(), {}
    def visit(n):
        block.extend(words(1)+n['name'].encode()+b'\0')
        block.extend(bytes(-len(block) % 4))
        for key, value in n['props']:
            if key not in offsets:
                offsets[key] = len(table); table.extend(key.encode()+b'\0')
            block.extend(words(3,len(value),offsets[key])+value)
            block.extend(bytes(-len(block) % 4))
        for child in n['children']:
            visit(child)
        block.extend(words(2))
    visit(tree); block.extend(words(9))
    start = 56; strings = start+len(block)
    return words(0xd00dfeed,strings+len(table),start,strings,40,17,16,0,len(table),len(block))+bytes(16)+block+table


def main():
    root = Path('.build/acpi-probe'); root.mkdir(parents=True, exist_ok=True)
    tree = fixture(); (root/'good.dtb').write_bytes(pack(tree))
    gic = '/soc/interrupt-controller@0c000000'
    mutations = [('/', 'compatible', b'mediatek,other\0'), ('/', '#address-cells', words(1)),
        ('/soc','ranges',words(0,0,0,0,1,0)), ('/cpus','#address-cells',words(2)),
        ('/cpus','#size-cells',words(1)), ('/cpus/cpu@2','reg',words(0)),
        ('/cpus/cpu@3','reg',words(0x800)), ('/cpus/cpu@4','device_type',None),
        ('/cpus/cpu@5','enable-method',b'spin-table\0'), ('/cpus/cpu@6','compatible',b'arm,cortex-a55\0'),
        ('/psci','method',b'hvc\0'), ('/timer','interrupt-parent',words(1)),
        ('/timer','interrupts',words(1,13,4,1,14,4,1,11,4,1,10,4)),
        ('/pmu','interrupts',words(0,7,8)), (gic,'#interrupt-cells',words(4)),
        (gic,'phandle',words(0)), (gic,'#redistributor-regions',words(2)),
        (gic,'redistributor-stride',words(0,0x40000)), (gic,'reg',words(0,0x10000000,0,0x1000))]
    bad = []
    for path,key,value in mutations:
        t=deepcopy(tree); replace(t,path,key,value); bad.append(pack(t))
    for path in ('/soc','/cpus','/cpus/cpu@0','/timer','/pmu','/psci',gic):
        t=deepcopy(tree); replace(t,path,'status',b'disabled\0'); bad.append(pack(t))
    t=deepcopy(tree); get(t,'/cpus')['children'].pop(); bad.append(pack(t))
    t=deepcopy(tree); get(t,'/cpus')['children'].append(deepcopy(get(t,'/cpus/cpu@0'))); bad.append(pack(t))
    t=deepcopy(tree); get(t,gic)['props'].append(('reg',words(0)*12)); bad.append(pack(t))
    t=deepcopy(tree); get(t,'/cpus/cpu@1')['props'].append(('reg',words(0x100))); bad.append(pack(t))
    bad.extend((pack(tree)[:-1], bytes(40)))
    for i,data in enumerate(bad):
        (root/f'bad-{i:02}.dtb').write_bytes(data)
    print(f'Generated{len(bad)} malformed/unsupported synthetic FDTs')


if __name__ == '__main__':
    main()
