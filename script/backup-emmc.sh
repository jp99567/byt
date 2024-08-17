#!/bin/bash

set -ex

if [[ -n "$1" ]]; then
	destdir="$1"
else
	mntdir=/mnt/w
	mountpoint $mntdir || mount vivo:/home/j/w $mntdir
	destdir=$mntdir/bb-img-backup
fi

[[ -d "$destdir" ]]

indev=/dev/mmcblk1
mac=$(ip link show eth0|grep -oP 'ether .+ brd'|grep -oP '..:..:..:..:..'|tr -d :)
ofile="bb-mac${mac}-$(date +"%F_%T").img"
dd if=$indev of=$destdir/$ofile bs=1M status=progress
