#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
set -euo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
edk2="${EDK2_HEADERS:?Set EDK2_HEADERS to reviewed EDK2 sources}"
mkdir -p .build/msdc
cat > .build/msdc/profile.dts <<'EOF'
/dts-v1/;
/ {
  compatible = "mediatek,MT6779";
  #address-cells = <2>; #size-cells = <2>;
  soc {
    #address-cells = <2>; #size-cells = <2>;
    msdc@11240000 {
      compatible = "mediatek,msdc"; reg = <0 0x11240000 0 0x10000>;
      host_function = /bits/ 8 <1>; bus-width = <4>; status = "okay";
      vmmc-supply = <&vmch>; vqmmc-supply = <&vmc>;
    };
    clock-controller@10000000 {
      compatible = "mediatek,mt6779-topckgen", "syscon";
      reg = <0 0x10000000 0 0x1000>;
    };
    clock-reset-controller@10001000 {
      compatible = "mediatek,common-infracfg_ao", "mediatek,mt6779-infracfg_ao", "syscon";
      reg = <0 0x10001000 0 0x1000>;
    };
    pinctrl@10005000 {
      compatible = "mediatek,mt6779-pinctrl", "syscon";
      reg = <0 0x10005000 0 0x1000 0 0x11c20000 0 0x1000>;
    };
    iocfg_rm@11c20000 {
      compatible = "mediatek,iocfg_rm", "syscon";
      reg = <0 0x11c20000 0 0x1000>;
    };
    i2c_common {
      compatible = "mediatek,i2c_common";
      ver = /bits/ 8 <2>; set_dt_div = /bits/ 8 <1>;
      set_ltiming = /bits/ 8 <1>; cnt_constraint = /bits/ 8 <1>;
    };
    i2c5@11016000 {
      compatible = "mediatek,i2c"; reg = <0 0x11016000 0 0x1000>;
      id = <5>; clock-div = <5>; scl-gpio-id = <28>; sda-gpio-id = <29>;
      mediatek,use-push-pull;
      #address-cells = <1>; #size-cells = <0>;
      subpmic_ldo@64 {
        compatible = "mediatek,subpmic_ldo"; reg = <0x64>;
        vmc: ldo3 {regulator-name = "VMC"; regulator-min-microvolt = <1200000>; regulator-max-microvolt = <3600000>;};
        vmch: ldo5 {regulator-name = "VMCH"; regulator-min-microvolt = <2700000>; regulator-max-microvolt = <3600000>;};
      };
    };
  };
};
EOF
dtc -I dts -O dtb .build/msdc/profile.dts -o .build/msdc/profile.dtb
gcc -std=c11 -Wall -Wextra -Werror -fshort-wchar -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$edk2/MdePkg/Include" -I"$edk2/MdePkg/Include/X64" -IOppoReno5LitePkg/Include \
  OppoReno5LitePkg/Library/OppoHandoffLib/SdProfile.c \
  OppoReno5LitePkg/Library/OppoFdtLib/OppoFdtLib.c \
  scripts/test_sd_profile.c -o .build/msdc/test-sd-profile
.build/msdc/test-sd-profile .build/msdc/profile.dtb "$@"
