#!/bin/bash

set -e

config-pin -f config-pin.txt

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

