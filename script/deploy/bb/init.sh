#!/bin/bash

set -e

CONFIGSCRIPT=./avrcanconf/run.py
[[ -x "$CONFIGSCRIPT" ]]

RETRIES_TIMEOUT=20
while ! ip link show type can|grep -q $BYTD_CAN_DEVICE_NAME; do
	if [[ $RETRIES_TIMEOUT -gt 0 ]]; then
		echo "wait for can devices.. ($RETRIES_TIMEOUT)"
		sleep 1
	else
		echo No can devices
		exit 1
	fi
	RETRIES_TIMEOUT=$(( $RETRIES_TIMEOUT - 1 ))
done

if ip link show $BYTD_CAN_DEVICE_NAME|grep -q 'state DOWN'; then
	ip link set $BYTD_CAN_DEVICE_NAME type can bitrate 100000
	ip link set up $BYTD_CAN_DEVICE_NAME
fi

P9_28_CHIP=gpiochip3
P9_28_LINE=17
gpioset --daemonize --chip $P9_28_CHIP $P9_28_LINE=1

$CONFIGSCRIPT --exit_bootloader --candev $BYTD_CAN_DEVICE_NAME --all
$CONFIGSCRIPT --config --yamlfile config.yaml --candev $BYTD_CAN_DEVICE_NAME

RPROC_CTRL_FILE='/sys/class/remoteproc/remoteproc1/state'
while [[ ! -f $RPROC_CTRL_FILE ]]; do
	echo waiting for $RPROC_CTRL_FILE
	sleep 5
done
RPROC_CUR_STATE=$(<$RPROC_CTRL_FILE)
if [[ 'offline' != $RPROC_CUR_STATE ]]; then
	echo "$RPROC_CTRL_FILE is not offline ($RPROC_CUR_STATE). Stopping"
	echo stop > $RPROC_CTRL_FILE
fi

echo start > $RPROC_CTRL_FILE

RPROC_DEV='/dev/rpmsg_pru30'
RETRIES_TIMEOUT=10
while [[ ! -c $RPROC_DEV ]]; do
	if [[ $RETRIES_TIMEOUT -gt 0 ]]; then
		echo "waiting for $RPROC_DEV ($RETRIES_TIMEOUT)"
		sleep 1
	else
		echo No $RPROC_DEV device
		exit 1
	fi
	RETRIES_TIMEOUT=$(( $RETRIES_TIMEOUT - 1 ))
done
