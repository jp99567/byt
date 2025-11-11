#!/bin/bash

set -ex
if [[ $(</sys/class/remoteproc/remoteproc1/state) != "offline" ]]; then
	echo stop > /sys/class/remoteproc/remoteproc1/state
fi
cp /mnt/w/byt/pru/gen/pru.out /lib/firmware/am335x-pru0-fw
echo start > /sys/class/remoteproc/remoteproc1/state
