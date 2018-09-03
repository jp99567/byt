#!/bin/bash

set -e

BASE_GPIO_DIR='/run/bytd/gpio'
mkdir -p $BASE_GPIO_DIR
cd $BASE_GPIO_DIR

function mkGpioLink {
	sysfs_path="/sys/class/gpio/gpio$1"
	symname=$2

	if [[ ! -L $symname ]]; then
		ln -s $sysfs_path $symname
	fi
}

while read i; do
	mkGpioLink $i
done <<GPIOLIST
117 denon
119 zvoncek
121 kotol
GPIOLIST
