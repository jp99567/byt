
cmake_minimum_required(VERSION 3.5)
project (st2cpp)

find_package(Qt5 COMPONENTS Core REQUIRED)
include_directories(../../spdlog/include)
#link_directories(${Thrift_LIBRARY_DIRS})
file(GLOB_RECURSE SRC_FILES FOLLOW_SYMLINKS *.cpp)
add_executable (st2cpp ${SRC_FILES})
target_link_libraries(st2cpp ${Qt5Core_LIBRARIES} pthread)
set(CMAKE_CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -flto")
set_property(TARGET st2cpp PROPERTY CXX_STANDARD 17)

