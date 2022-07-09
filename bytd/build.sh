
set -ex

REPO=$(git rev-parse --show-toplevel)

cd $REPO
SRC_DIR=$REPO/bytd
. $SRC_DIR/env.sh
[[ -d $CROSS_ENV_ROOT ]]
BUILD_DIR=$REPO/build-bytd
mkdir -p $BUILD_DIR
cd $BUILD_DIR
cmake $SRC_DIR -DCMAKE_TOOLCHAIN_FILE=$REPO/toolchain-bb.cmake
make -j`nproc`

