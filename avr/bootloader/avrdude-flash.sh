#!/bin/bash

set -e 

[[ $(basename `pwd`) == "bootloader" ]]
avr-g++ --version

make clean
make ID=$1
ssh root@bbb3 -q avrdude -c linuxspi -P /dev/spidev1.0:/dev/gpiochip3 -p c32 -U signature:r:/dev/null:i
ssh root@bbb3 -q avrdude -c linuxspi -P /dev/spidev1.0:/dev/gpiochip3 -p c32 -U lfuse:w:0xcd:m -U hfuse:w:0xde:m -U efuse:w:0xf5:m
ssh root@bbb3 -q avrdude -c linuxspi -P /dev/spidev1.0:/dev/gpiochip3 -p c32 -U flash:w:/mnt/w/byt/avr/bootloader/bootloader.hex:i
echo  $1 OK
