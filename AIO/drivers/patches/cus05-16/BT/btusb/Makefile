# need set the following enviroment, do not change this file if build in AIO
#export ARCH=arm64
#export CROSS_COMPILE=/work/android_path/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
#export ARCH=arm
#export CROSS_COMPILE=/work/android_path/prebuilts/gcc/linux-x86/arm/arm-2014.05/sbin/arm-none-linux-gnueabi-201405-
#KDIR := /work/android_path/kernel

obj-m := SS1BTUSB.o
PWD := $(shell pwd)
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf modules.order Module.symvers SS1BTUSB.ko SS1BTUSB.mod.c SS1BTUSB.mod.o SS1BTUSB.o

