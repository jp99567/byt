#!/bin/bash

set -e

id=$1
echo flashing avr node:$id

config-pin p9.28 low
sleep 0.1
config-pin p9.28 high
sleep 0.1
./avrcanconf.py --fw_upload --candev can1 --id $id
./avrcanconf.py --exit_bootloader --candev can1 --all
./avrcanconf.py --config --yamlfile config.yaml  --candev can1
./avrcanconf.py --cmd CmdSetStage --candev can1 --id $id 0b11000000
