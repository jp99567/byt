#!/bin/bash

set -e

config-pin -f config-pin.txt

if ip link show can1|grep -q 'state DOWN'; then
	ip link set can1 type can bitrate 100000
	ip link set up can1
fi

if config-pin -q 'p9.28'|grep -q 'P9_28 Mode: gpio Direction: out Value: 1'; then
	config-pin p9.28 low
	sleep 0.2
fi
config-pin p9.28 high

./avrcanconf.py --exit_bootloader --candev can1 --all
./avrcanconf.py --config --yamlfile config.yaml --candev can1

RPROC_CTRL_FILE='/sys/class/remoteproc/remoteproc1/state'
while [[ ! -f $RPROC_CTRL_FILE ]]; do
	echo waiting for $RPROC_CTRL_FILE
	sleep 5
done
RPROC_CUR_STATE=$(<$RPROC_CTRL_FILE)
if [[ 'offline' != $RPROC_CUR_STATE ]]; then
	echo $RPROC_CTRL_FILE $RPROC_CUR_STATE
	echo stop > $RPROC_CTRL_FILE
fi

echo start > $RPROC_CTRL_FILE

