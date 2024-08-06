#!/bin/bash

set -ex

id=$1
CONFIGSCRIPT=./avrcanconf/run.py
[[ -x "$CONFIGSCRIPT" ]]
echo flashing avr node:$id

config-pin p9.28 low
sleep 2
config-pin p9.28 high
sleep 0.1
$CONFIGSCRIPT --fw_upload --candev can1 --id $id
$CONFIGSCRIPT --exit_bootloader --candev can1 --all
$CONFIGSCRIPT --config --yamlfile config.yaml  --candev can1
$CONFIGSCRIPT --cmd CmdSetStage --candev can1 --all 0b11000000
