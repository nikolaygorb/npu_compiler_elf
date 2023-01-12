#
# Copyright Intel Corporation.
#
# LEGAL NOTICE: Your use of this software and any required dependent software
# (the "Software Package") is subject to the terms and conditions of
# the Intel(R) OpenVINO(TM) Distribution License for the Software Package,
# which may also include notices, disclaimers, or license terms for
# third party or open source software included in or with the Software Package,
# and your use indicates your acceptance of all such terms. Please refer
# to the "third-party-programs.txt" or other similarly-named text file
# included with the Software Package for additional details.
#

cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

include( ./cmake/compiler_linker_options.cmake )

set(LIB_NAME vpux_elf)

project(${LIB_NAME})

include(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    message(FATAL "ELF Parsing Library supports only little-endian architectures")
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/example/.*")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HEADERS} ${SOURCES})

add_library(${LIB_NAME} STATIC ${SOURCES})
target_include_directories(${LIB_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/loader/include>)

# Set compiler options
target_compile_options(${LIB_NAME} PRIVATE
    $<$<CONFIG:Release>:${UMD_COMPILER_OPTIONS_RELEASE}>
    $<$<CONFIG:Debug>:${UMD_COMPILER_OPTIONS_DEBUG}>
    $<$<CONFIG:RelWithDebInfo>:${UMD_COMPILER_OPTIONS_RELEASE}>)

# Set linker options
target_link_options(${LIB_NAME} PRIVATE
    $<$<CONFIG:Release>:${UMD_LINKER_OPTIONS_RELEASE}>
    $<$<CONFIG:Debug>:${UMD_LINKER_OPTIONS_DEBUG}>
    $<$<CONFIG:RelWithDebInfo>:${UMD_LINKER_OPTIONS_RELEASE}>)

if(NOT BUILD_SHARED_LIBS)
    install(TARGETS ${LIB_NAME}
            RUNTIME DESTINATION ${IE_CPACK_RUNTIME_PATH} COMPONENT ${VPUX_PLUGIN_COMPONENT}
            ARCHIVE DESTINATION ${IE_CPACK_ARCHIVE_PATH} COMPONENT ${VPUX_PLUGIN_COMPONENT}
            LIBRARY DESTINATION ${IE_CPACK_LIBRARY_PATH} COMPONENT ${VPUX_PLUGIN_COMPONENT})
endif()
