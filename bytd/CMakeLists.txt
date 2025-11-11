
cmake_minimum_required(VERSION 3.21)
project(bytd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
#find_package(mosquittopp REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGPIODCXX REQUIRED IMPORTED_TARGET libgpiodcxx=1.6.5)

option(SIMULATOR "connected to test interfaces" HOST )

find_package(Git REQUIRED)
if(Git_FOUND)
    execute_process(
       COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
       RESULT_VARIABLE GIT_COMMAND_RESULT
       OUTPUT_VARIABLE GIT_REV_DESC_STR
       OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message("git result: ${GIT_COMMAND_RESULT}")
endif()
message("git rev: ${GIT_REV_DESC_STR}")
configure_file(git_revision_description.h.in inc_generated/git_revision_description.h)

set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_CXX_FLAGS "-Wall -pthread -Wno-psabi")
IF(NOT CMAKE_CROSSCOMPILING)
	#include(FindPkgConfig)
	#    pkg_check_modules(SYSTEMDDAEMON REQUIRED libsystemd)
	#    pkg_check_modules(GPIOD REQUIRED libgpiod)
    option(UTEST "unit tests" FALSE)
    IF(UTEST)
        add_subdirectory(utest)
    ENDIF()
ENDIF()

file(GLOB_RECURSE SRC_CPP src/*.cpp)
file(GLOB_RECURSE SRC_HEADERS src/*.h)
add_executable(${PROJECT_NAME} ${SRC_CPP} ${SRC_HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LIBGPIODCXX Threads::Threads Boost::system systemd rt mosquittopp yaml-cpp stdc++fs fmt)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SYSROOT}/usr/local/include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/inc_generated
)
#set(CMAKE_CXX_FLAGS_RELEASE "-O2 -flto")
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

IF(SIMULATOR)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BYTD_SIMULATOR)
ENDIF()
