cmake_minimum_required(VERSION 3.20)

project(fw LANGUAGES C CXX ASM)

set(PRG_NAME fw)
set(MCU_TARGET at90can32)
set(BOOTSTART 0x0)
set(OPTIMIZE -O2)

file(GLOB CXX_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

# Build metadata from git and the current epoch.
set(FW_GIT_HASH_SHORT "00000000")
execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    OUTPUT_VARIABLE FW_GIT_HASH_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

execute_process(
    COMMAND git diff --quiet
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    RESULT_VARIABLE GIT_DIFF_RESULT
)
if(GIT_DIFF_RESULT EQUAL 0)
    set(FW_GIT_IS_DIRTY 0)
else()
    set(FW_GIT_IS_DIRTY 1)
endif()

set(FW_BUILD_EPOCH "0")
execute_process(
    COMMAND date +%s
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    OUTPUT_VARIABLE FW_BUILD_EPOCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

add_executable(${PRG_NAME} ${CXX_SOURCES})
set_target_properties(${PRG_NAME} PROPERTIES OUTPUT_NAME ${PRG_NAME})

set(COMMON_FLAGS
    -g
    -Wall
    ${OPTIMIZE}
    -mmcu=${MCU_TARGET}
    -flto
)

target_compile_features(${PRG_NAME} PRIVATE cxx_std_17)

target_compile_options(${PRG_NAME} PRIVATE ${COMMON_FLAGS})

target_compile_definitions(${PRG_NAME} PRIVATE
    FW_GIT_HASH_SHORT=0x${FW_GIT_HASH_SHORT}
    FW_GIT_IS_DIRTY=${FW_GIT_IS_DIRTY}
    FW_BUID_EPOCH=${FW_BUILD_EPOCH}
)

target_link_options(${PRG_NAME} PRIVATE
    -flto
    -Wl,-Map,${PRG_NAME}.map
    -Wl,--section-start=.text=${BOOTSTART}
)

find_program(AVR_OBJCOPY avr-objcopy)
find_program(AVR_OBJDUMP avr-objdump)

if(NOT AVR_OBJCOPY)
    message(FATAL_ERROR "avr-objcopy not found in PATH")
endif()
if(NOT AVR_OBJDUMP)
    message(FATAL_ERROR "avr-objdump not found in PATH")
endif()

add_custom_command(
    TARGET ${PRG_NAME}
    POST_BUILD
    COMMAND ${AVR_OBJDUMP} -h -S $<TARGET_FILE:${PRG_NAME}> > ${PRG_NAME}.lst
    COMMAND ${AVR_OBJCOPY} -j .text -j .data -O ihex $<TARGET_FILE:${PRG_NAME}> ${PRG_NAME}.hex
    COMMAND ${AVR_OBJCOPY} -j .text -j .data -O srec $<TARGET_FILE:${PRG_NAME}> ${PRG_NAME}.srec
    COMMAND ${AVR_OBJCOPY} -j .text -j .data -O binary $<TARGET_FILE:${PRG_NAME}> ${PRG_NAME}.bin
    COMMAND ${AVR_OBJCOPY} -j .eeprom --change-section-lma .eeprom=0 -O ihex $<TARGET_FILE:${PRG_NAME}> ${PRG_NAME}_eeprom.hex
            || ${CMAKE_COMMAND} -E echo "empty ${PRG_NAME}_eeprom.hex not generated"
    COMMAND ${AVR_OBJCOPY} -j .eeprom --change-section-lma .eeprom=0 -O srec $<TARGET_FILE:${PRG_NAME}> ${PRG_NAME}_eeprom.srec
            || ${CMAKE_COMMAND} -E echo "empty ${PRG_NAME}_eeprom.srec not generated"
    COMMAND ${AVR_OBJCOPY} -j .eeprom --change-section-lma .eeprom=0 -O binary $<TARGET_FILE:${PRG_NAME}> ${PRG_NAME}_eeprom.bin
            || ${CMAKE_COMMAND} -E echo "empty ${PRG_NAME}_eeprom.bin not generated"
    BYPRODUCTS
        ${PRG_NAME}.lst
        ${PRG_NAME}.hex
        ${PRG_NAME}.srec
        ${PRG_NAME}.bin
        ${PRG_NAME}_eeprom.hex
        ${PRG_NAME}_eeprom.srec
        ${PRG_NAME}_eeprom.bin
    COMMENT "Generating AVR images"
)
