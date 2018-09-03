
cmake_minimum_required(VERSION 3.5)
project (Bytd)

include(FindPkgConfig)
pkg_check_modules(SYSTEMDDAEMON libsystemd)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
include_directories(../../spdlog/include ../owtherm ${Thrift_INCLUDE_DIRS})
add_definitions(-DUSE_STD_THREAD)
#link_directories(${Thrift_LIBRARY_DIRS})
file(GLOB_RECURSE SRC_FILES FOLLOW_SYMLINKS *.cpp)
add_executable (bytd ${SRC_FILES})
target_link_libraries(bytd ${Boost_SYSTEM_LIBRARY} Threads::Threads thrift pq systemd)
set(CMAKE_CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -flto")
set_property(TARGET bytd PROPERTY CXX_STANDARD 14)

