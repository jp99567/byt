#!/bin/bash

set -ex

img_ident=$1

mntdir=/mnt/w
mountpoint $mntdir || mount vivo:/home/j/w $mntdir
destdir=$mntdir/bb-img-backup
[[ -d "$destdir" ]]
indev=/dev/mmcblk1
mac=$(ip link show eth0|grep -oP 'ether .+ brd'|grep -oP '..:..:..:..:..'|tr -d :)

if [[ -n "$img_ident" ]]; then
	tmpmnt="/tmp/$img_ident"
	mkdir $tmpmnt
	mount ${indev}p1 $tmpmnt
	cd $tmpmnt/boot
	backup_mark_file=backup_mark.txt
	backup_mark_history=backup_mark_history.txt
	if [[ -f $backup_mark_file ]]; then
		cat $backup_mark_file >> $backup_mark_history
	fi
	echo $img_ident > $backup_mark_file
	cd /
	umount $tmpmnt
else
	img_ident="0_$(date +"%F_%T")"
fi

#ofile="bb-mac${mac}-ident$(date +"%F_%T").img"
ofile="bb-mac${mac}-ident${img_ident}.img"
dd if=$indev of=$destdir/$ofile bs=1M status=progress
