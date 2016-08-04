#! /bin/bash
echo "Start to patch files"
mkdir temp
cd temp
wget https://source.codeaurora.org/quic/la/platform/hardware/qcom/wlan/snapshot/wlan-AU_LINUX_ANDROID_LA.BR.1.1.2_RB1.05.00.02.031.016.tar.gz
tar -zxvf wlan-AU_LINUX_ANDROID_LA.BR.1.1.2_RB1.05.00.02.031.016.tar.gz
cd ..

patch -p1 --no-backup-if-mismatch < 0001.WL-device.patch
patch -p1 --no-backup-if-mismatch < 0002.WL-frameworks.patch
patch -p1 --no-backup-if-mismatch < 0003.WL-hardware.patch
patch -p1 --no-backup-if-mismatch < 0004.WL-3.18-kernel.patch
patch -p1 --no-backup-if-mismatch < 0005.WL-wlan_recovery_support.patch
cp sysctl.conf out/target/product/x86/system/etc/
if [ ! -d "hardware/qcom" ]; then
	mkdir hardware/qcom
fi
if [ ! -d "hardware/qcom/wlan" ]; then
	mkdir hardware/qcom/wlan
fi
mv temp/wlan-AU_LINUX_ANDROID_LA.BR.1.1.2_RB1.05.00.02.031.016/qcwcn hardware/qcom/wlan/
rm -fr temp
echo "Done! patch completed!"
