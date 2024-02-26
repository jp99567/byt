#!/bin/bash

set -ex

if [[ -n "$1" ]]; then
	destdir="$1"
else
	mntdir=/mnt/w
	mountpoint $mntdir || mount x509fa:/home/j/w $mntdir
	destdir=$mntdir/bbb-img-backup
fi

[[ -d "$destdir" ]]

indev=/dev/mmcblk1
ofile="$(hostname)-$(date +"%F_%T").img"
dd if=$indev of=$destdir/$ofile bs=1M status=progress
