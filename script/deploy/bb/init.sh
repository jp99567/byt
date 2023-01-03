#!/bin/bash

set -e

config-pin -f config-pin.txt

RPROC_STATE='/sys/class/remoteproc/remoteproc1/state'
if [[ 'running' == $(<$RPROC_STATE) ]]; then
	echo stop > $RPROC_STATE
fi

echo start > $RPROC_STATE

