
cmake_minimum_required(VERSION 3.24)
project(bytd)

find_package(Threads REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(PostgreSQL REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_CXX_FLAGS "-Wall -pthread -Wno-psabi")
IF(NOT CMAKE_CROSSCOMPILING)
 include(FindPkgConfig)
 pkg_check_modules(SYSTEMDDAEMON REQUIRED libsystemd)
 pkg_check_modules(GPIOD REQUIRED libgpiod)
ENDIF()

file(GLOB_RECURSE SRC_FILES *.cpp)
add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} Threads::Threads Boost::system PostgreSQL::PostgreSQL systemd gpiodcxx rt mosquitto)
target_include_directories(${PROJECT_NAME} PRIVATE ${PostgreSQL_INCLUDE_DIRS})
#set(CMAKE_CXX_FLAGS_RELEASE "-O2 -flto")
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
