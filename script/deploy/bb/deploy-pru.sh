#!/bin/bash

set -ex
echo stop > /sys/class/remoteproc/remoteproc1/state
cp /mnt/w/byt/pru/gen/pru.out /lib/firmware/am335x-pru0-fw
echo start > /sys/class/remoteproc/remoteproc1/state
