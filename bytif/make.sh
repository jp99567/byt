#/bin/bash

set -e

mkdir -p gen-req
mkdir -p gen-ind
thrift -r -gen cpp -out gen-ind indication.thrift
thrift -r -gen cpp -out gen-req request.thrift
rm gen-req/*.skeleton.cpp gen-ind/*.skeleton.cpp
