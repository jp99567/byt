
cmake_minimum_required(VERSION 3.5)
project (Bytd)
INCLUDE (FindPkgConfig)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
pkg_search_module(Thrift REQUIRED thrift)
find_package(Boost COMPONENTS system REQUIRED)
include_directories(../../spdlog-0.14.0/include ${Thrift_INCLUDE_DIRS})
link_directories(${Thrift_LIBRARY_DIRS})
file(GLOB_RECURSE SRC_FILES FOLLOW_SYMLINKS *.cpp)
add_executable (bytd ${SRC_FILES})
target_link_libraries(bytd ${Boost_SYSTEM_LIBRARY} Threads::Threads thrift)
set(CMAKE_BUILD_TYPE Debug)
set_property(TARGET bytd PROPERTY CXX_STANDARD 14)

