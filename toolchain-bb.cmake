# this one is important
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR arm) 
#this one not so much
SET(CMAKE_SYSTEM_VERSION 1)

# specify the cross compiler
SET(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc-12)
SET(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++-12)

IF(NOT DEFINED ENV{CROSS_ENV_ROOT})
	MESSAGE(FATAL_ERROR "not defined env CROSS_ENV_ROOT" $ENV{CROSS_ENV_ROOT})
ENDIF()
MESSAGE("cross build toolchain bbroot: " $ENV{CROSS_ENV_ROOT})

# where is the target environment
SET(CMAKE_SYSROOT $ENV{CROSS_ENV_ROOT})
SET(CMAKE_FIND_ROOT_PATH $ENV{CROSS_ENV_ROOT})

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_FIND_ROOT_PATH}/usr/lib/pkgconfig:${CMAKE_FIND_ROOT_PATH}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_FIND_ROOT_PATH})

