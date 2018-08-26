cmake-gui --trace \
	-D__UNIX_PATHS_INCLUDED=1 \
	-DCMAKE_SYSTEM_NAME=Android \
	-DCMAKE_ANDROID_ARCH_ABI=armeabi-v7a \
	-DCMAKE_ANDROID_NDK=/home/j/Android/Sdk/ndk-bundle \
	-DCMAKE_SYSTEM_VERSION=24 \
	-DCMAKE_CXX_ANDROID_TOOLCHAIN_PREFIX=/home/j/Android/Sdk/ndk-bundle/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi- \
	-DBOOST_INCLUDEDIR=/usr/include \
 -DBUILD_COMPILER:BOOL=OFF \
 -DBUILD_CPP:BOOL=ON \
 -DBUILD_LIBRARIES:BOOL=ON \
 -DCMAKE_BUILD_TYPE:STRING=Release \
 -DCMAKE_INSTALL_PREFIX:PATH=/usr/local \
 -DWITH_BOOST_FUNCTIONAL:BOOL=OFF \
 -DWITH_BOOST_SMART_PTR:BOOL=OFF \
 -DWITH_BOOST_STATIC:BOOL=OFF \
 -DWITH_CPP:BOOL=ON \
 -DWITH_C_GLIB:BOOL=OFF \
 -DWITH_HASKELL:BOOL=OFF \
 -DWITH_JAVA:BOOL=OFF \
 -DWITH_PYTHON:BOOL=OFF \
 -DWITH_SHARED_LIB:BOOL=OFF \
 -DWITH_STATIC_LIB:BOOL=ON \
 -DWITH_STDTHREADS:BOOL=ON \
 -DWITH_ZLIB:BOOL=OFF \
       	../../thrift
	

#-c -D__ANDROID_API__=16
#--sysroot=/home/j/Android/Sdk/ndk-bundle/sysroot
#-isystem /home/j/Android/Sdk/ndk-bundle/sysroot/usr/include/arm-linux-androideabi
#-isystem /home/j/Android/Sdk/ndk-bundle/sources/cxx-stl/gnu-libstdc++/4.9/include
#-isystem /home/j/Android/Sdk/ndk-bundle/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi-v7a/include
#-fstack-protector-strong
#-DANDROID
#-march=armv7-a
#-mfloat-abi=softfp -mfpu=vfp -fno-builtin-memmove -mthumb -Os -std=gnu++14 -Wall -W -D_REENTRANT -fPIC
