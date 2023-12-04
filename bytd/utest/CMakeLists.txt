
Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)

FetchContent_MakeAvailable(Catch2)

add_executable(tests mqtt_topic.cpp ../src/OwTempSensor.cpp ../src/SimpleSensor.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain fmt)
target_include_directories(tests PRIVATE ../src/ ${CMAKE_SOURCE_DIR}/..)
target_compile_definitions(tests PRIVATE WITHOUT_SPDLOG)
