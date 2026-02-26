cmake_minimum_required(VERSION 3.28)
project(bytd_utest LANGUAGES C CXX)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)

FetchContent_MakeAvailable(Catch2)

add_executable(tests
    mqtt_topic.cpp
    ../src/OwTempSensor.cpp
    ../src/SimpleSensor.cpp
    ../src/Kurenie.cpp
    ../src/candata.cpp
    ../src/canbus.cpp
    ../src/hwif.cpp
    ../src/OnOffDevice.cpp
    kurenie.cpp
    svetla.cpp
)

target_link_libraries(tests PRIVATE Catch2::Catch2WithMain fmt)
target_include_directories(tests PRIVATE ../src/ ${CMAKE_SOURCE_DIR}/.. ${CMAKE_SOURCE_DIR}/../..)
target_compile_definitions(tests PRIVATE WITHOUT_SPDLOG)
