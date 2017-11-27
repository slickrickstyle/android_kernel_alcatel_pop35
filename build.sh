#!/bin/sh
export KERNELDIR=`readlink -f .`
export CROSS_COMPILE=/home/slickrickstyle/android/toolchains/linaro-arm-linux-gnueabi-4.9/bin/arm-linux-gnueabi-
export ARCH=arm
if [ ! -f $KERNELDIR/.config ];
then
  make defconfig test-pop35_defconfig
fi
. $KERNELDIR/.config
# mv .git .git-halt
echo "Clearing DTB files ..."
rm $KERNELDIR/arch/arm/boot/dts/*.dtb
echo "Cross-compiling kernel ..."
cd $KERNELDIR/
make -j3 || exit 1
rm -rf $KERNELDIR/BUILT_pixi35
mkdir -p $KERNELDIR/BUILT_pixi35/lib/modules
find -name '*.ko' -exec mv -v {} $KERNELDIR/BUILT_pixi35/lib/modules/ \;
echo ""
echo "Stripping unneeded symbols and debug info from module(s)..."
${CROSS_COMPILE}strip --strip-unneeded $KERNELDIR/BUILT_pixi35/lib/modules/*
mkdir -p $KERNELDIR/BUILT_pixi35/lib/modules/pronto
mv $KERNELDIR/BUILT_pixi35/lib/modules/wlan.ko $KERNELDIR/BUILT_pixi35/lib/modules/pronto/pronto_wlan.ko
mv $KERNELDIR/arch/arm/boot/zImage $KERNELDIR/BUILT_pixi35/
echo ""
echo "Compiling >> dtbtool <<  for device tree image ..."
cd $KERNELDIR/tools/dtbtool; make
echo ""
echo "Generating Device Tree image (dt.img) ..."
$KERNELDIR/tools/dtbtool/dtbtool -o $KERNELDIR/BUILT_pixi35/dt.img -s 2048 -p $KERNELDIR/scripts/dtc/ $KERNELDIR/arch/arm/boot/dts/ && rm $KERNELDIR/tools/dtbtool/dtbtool
cd $KERNELDIR/; # mv .git-halt .git
echo ""
echo "BUILT_pixi35 is ready."
