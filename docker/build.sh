#!/bin/bash

set -e

pushd /repo
BDIR=build-bytd-cross-deb13
if [[ -f "$(find avr -name '*generated.h' -print -quit)" ]]; then
	python3 script/deploy/bb/avrcanconf/run.py --generate
fi
mkdir -p $BDIR
pushd $BDIR
cmake -G Ninja --toolchain ../toolchain-bb.cmake ../bytd
cmake --build .
