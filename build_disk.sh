#!/bin/bash

ROOT_DIR=hdd
IMAGE=dionysus.img
HEADS=5
SPH=17
CYLINDERS=980
PARTITION_START=2048

die() {
	if [[ ! -z $LOOPDEV ]]; then
		sudo losetup -d $LOOPDEV
	fi

	if [[ ! -z $TMP ]]; then
		rm $TMP
	fi

	exit
}

echo "Creating image"
dd if=/dev/zero of=$IMAGE bs=512 count=$PARTITION_START || die
TMP=$(tempfile) || die
genext2fs -d $ROOT_DIR -U -b $(expr \( $HEADS \* $SPH \* $CYLINDERS - $PARTITION_START \) / 2) -N 4096 $TMP || die
cat $TMP >> $IMAGE
rm $TMP || die

echo "Creating partition table"
LOOPDEV=$(sudo losetup -f --show $IMAGE) || die
sudo sfdisk -uS $LOOPDEV << EOF
$PARTITION_START,,,*
EOF
sudo losetup -d $LOOPDEV || die
LOOPDEV=

echo "Installing grub"
sudo grub --batch --device-map=/dev/null << EOF
device (hd0) $IMAGE
geometry (hd0) $CYLINDERS $HEADS $SPH
root (hd0,0)
setup (hd0)
quit
EOF
