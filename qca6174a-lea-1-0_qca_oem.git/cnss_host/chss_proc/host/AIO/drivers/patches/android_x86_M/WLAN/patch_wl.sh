#! /bin/bash
echo "Start to patch files"
patch -p1 --no-backup-if-mismatch < 0001.WL-device.patch
patch -p1 --no-backup-if-mismatch < 0002.WL-frameworks.patch
patch -p1 --no-backup-if-mismatch < 0003.WL-hardware.patch
patch -p1 --no-backup-if-mismatch < 0004.WL-kernel.patch
patch -p1 --no-backup-if-mismatch < 0005.WL-3.10-kernel-tcp-rx-tput-fix.patch
cp sysctl.conf out/target/product/x86/system/etc/
echo "Done! patch completed!"
