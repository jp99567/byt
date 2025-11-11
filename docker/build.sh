#!/bin/bash

set -e

pushd /repo
BDIR=build-bytd-cross-deb13
mkdir -p $BDIR
pushd $BDIR
cmake -G Ninja --toolchain ../toolchain-bb.cmake ../bytd
cmake --build .
