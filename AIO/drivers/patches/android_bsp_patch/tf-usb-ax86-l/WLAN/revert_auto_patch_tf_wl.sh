#! /bin/bash
patch -R -p1 < 0001.WL-device.patch
patch -R -p1 < 0002.WL-frameworks.patch
patch -R -p1 < 0003.WL-hardware.patch
patch -R -p1 < 0004.WL-3.18-kernel.patch
patch -R -p1 < 0005.WL-wlan_recovery_support.patch
rm -rf hardware/qcom/wlan/qcwcn/
