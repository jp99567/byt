#!/bin/bash

set -ex

id=$1
CONFIGSCRIPT=./avrcanconf/run.py
[[ -x "$CONFIGSCRIPT" ]]
echo flashing avr node:$id

P9_28_CHIP=3
P9_28_LINE=17
P9_28_PID=/run/gpio_p9_28.pid
gpioinfo $P9_28_CHIP|grep -P "line\s+$P9_28_LINE:"|grep -q unused # test if unused
gpioset --mode=signal $P9_28_CHIP $P9_28_LINE=1 &
echo $! > $P9_28_PID

sleep 0.1
$CONFIGSCRIPT --fw_upload --candev can1 --id $id
$CONFIGSCRIPT --exit_bootloader --candev can1 --all
$CONFIGSCRIPT --config --yamlfile config.yaml  --candev can1
$CONFIGSCRIPT --cmd CmdSetStage --candev can1 --all 0b11000000

kill $(<$P9_28_PID)
rm $P9_28_PID
sleep 0.1
