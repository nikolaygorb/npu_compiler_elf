#
# Copyright (C) 2023 Intel Corporation
# SPDX-License-Identifier: Apache 2.0
#

cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(version_major 0)
set(version_minor 0)
set(version_patch 1)
set(version "${version_major}.${version_minor}.${version_patch}")

project(elf
    VERSION ${version}
    DESCRIPTION "NPU implementation of ELF Loader"
    # HOMEPAGE_URL is ommited intentionally, as setting innersource version
    # would be a potential issue with open-sourcing
    LANGUAGES CXX
)

if (NOT "${CMAKE_CXX_BYTE_ORDER}" STREQUAL "LITTLE_ENDIAN")
    message(FATAL_ERROR "${PROJECT_NAME} supports LITTLE_ENDIAN only, detected ${CMAKE_CXX_BYTE_ORDER}")
endif()

include(GNUInstallDirs)

# consider encapsulate this into dependant targets
include( ./cmake/compiler_linker_options.cmake )

set(LIB_NAME npu_elf)
include(cmake/FindFirmwareHeaders.cmake)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
list(FILTER SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/example/.*")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HEADERS} ${SOURCES})

add_library(${LIB_NAME} STATIC ${SOURCES})
add_library(vpux_elf STATIC ${SOURCES})

if(MSVC AND BUILD_COMPILER_FOR_DRIVER)
    # For compiler in driver build, npu_elf or vpux_elf static lib needs to build with /MT to avoid mismatch with level zero targets.
    # From cmake >= 3.15, the default CMAKE_MSVC_RUNTIME_LIBRARY is /MD, thus causing always overriding /MT with /MD.
    # So here we set the target's MSVC_RUNTIME_LIBRARY to MT to avoid such overriding behavior.
    set_target_properties(${LIB_NAME} vpux_elf PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_compile_definitions(HOST_BUILD)

target_include_directories(${LIB_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/loader/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/3720>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/4000>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/include>)

target_include_directories(vpux_elf PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/loader/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/3720>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/4000>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/include>)

target_include_directories(${LIB_NAME} PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/include>)
target_include_directories(vpux_elf PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/include>)

# Set compiler options
target_compile_options(${LIB_NAME} PRIVATE
    $<$<CONFIG:Release>:${UMD_COMPILER_OPTIONS_RELEASE}>
    $<$<CONFIG:Debug>:${UMD_COMPILER_OPTIONS_DEBUG}>
    $<$<CONFIG:RelWithDebInfo>:${UMD_COMPILER_OPTIONS_RELEASE}>
    # /Zf enables compiler support for faster generation of PDB files when compiled concurrently
    $<$<CXX_COMPILER_ID:MSVC>:/Zf>)

target_compile_options(vpux_elf PRIVATE
    $<$<CONFIG:Release>:${UMD_COMPILER_OPTIONS_RELEASE}>
    $<$<CONFIG:Debug>:${UMD_COMPILER_OPTIONS_DEBUG}>
    $<$<CONFIG:RelWithDebInfo>:${UMD_COMPILER_OPTIONS_RELEASE}>
    # see ${LIB_NAME} compile options
    $<$<CXX_COMPILER_ID:MSVC>:/Zf>)

# Set linker options
target_link_options(${LIB_NAME} PRIVATE
    $<$<CONFIG:Release>:${UMD_LINKER_OPTIONS_RELEASE}>
    $<$<CONFIG:Debug>:${UMD_LINKER_OPTIONS_DEBUG}>
    $<$<CONFIG:RelWithDebInfo>:${UMD_LINKER_OPTIONS_RELEASE}>)

target_link_options(vpux_elf PRIVATE
    $<$<CONFIG:Release>:${UMD_LINKER_OPTIONS_RELEASE}>
    $<$<CONFIG:Debug>:${UMD_LINKER_OPTIONS_DEBUG}>
    $<$<CONFIG:RelWithDebInfo>:${UMD_LINKER_OPTIONS_RELEASE}>)

# seems to be mono-repo thing
# Add external deps
if(EXTERNAL_DEPS)
    target_link_libraries(${LIB_NAME} PUBLIC ${EXTERNAL_DEPS})
    target_link_libraries(vpux_elf PUBLIC ${EXTERNAL_DEPS})
    add_subdirectory(loader)
    if (UNIX)
        # we need for fPIC flags because `npu_elf` is used for dynamic libraries linkage.
        # it seems that this option is set implicitly somewhere due to compile flags overriding...
        # Since `vpux-elf` is expected to be a standalone submodule, we add this option by a purpose
        set_property(TARGET ${LIB_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
        set_property(TARGET vpux_elf PROPERTY POSITION_INDEPENDENT_CODE ON)
    endif()
endif(EXTERNAL_DEPS)

if(NOT BUILD_SHARED_LIBS)
    install(TARGETS ${LIB_NAME} vpux_elf
            RUNTIME DESTINATION ${IE_CPACK_RUNTIME_PATH} COMPONENT ${VPUX_PLUGIN_COMPONENT}
            ARCHIVE DESTINATION ${IE_CPACK_ARCHIVE_PATH} COMPONENT ${VPUX_PLUGIN_COMPONENT}
            LIBRARY DESTINATION ${IE_CPACK_LIBRARY_PATH} COMPONENT ${VPUX_PLUGIN_COMPONENT})
endif()

# This cmake code snippet should always be put at the end to make sure it's not updated finally.
if(MSVC AND BUILD_COMPILER_FOR_DRIVER)
    # To avoid the RuntimeLibrary mismatch in #E-109895 and #E-111901
    # Check targets's msvc runtime for 1click pipeline flow which isn't covered in NPU plugin's repo pr-check
    set(MODULE_DIR "build-modules")
    if(NOT ${PROJECT_BINARY_DIR} MATCHES ${MODULE_DIR})
        get_target_property(LIB_MSVC_RUNTIME_LIBRARY ${LIB_NAME} MSVC_RUNTIME_LIBRARY)
        get_target_property(VPUX_ELF_LIB_MSVC_RUNTIME_LIBRARY vpux_elf MSVC_RUNTIME_LIBRARY)
        if(NOT LIB_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreaded$<$<CONFIG:Debug>:Debug>")
            message(FATAL_ERROR "${LIB_NAME} CMAKE_MSVC_RUNTIME_LIBRARY is not MT[d], is ${LIB_MSVC_RUNTIME_LIBRARY}, exiting")
        endif()
        if(NOT VPUX_ELF_LIB_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreaded$<$<CONFIG:Debug>:Debug>")
            message(FATAL_ERROR "vpux_elf CMAKE_MSVC_RUNTIME_LIBRARY is not MT[d], is ${VPUX_ELF_LIB_MSVC_RUNTIME_LIBRARY}, exiting")
        endif()
    endif()
endif()


set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
add_library(${PROJECT_NAME} SHARED)
target_compile_definitions(${PROJECT_NAME} PUBLIC ELF_EXPORTS)


target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/loader/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/3720>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/4000>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/include>
)

target_include_directories(${PROJECT_NAME} SYSTEM INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/4000>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/3720>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hpi_component/include/common>

    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/loader/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/include>
)

target_sources(${PROJECT_NAME}
    PRIVATE
        core/src/types/relocation_entry.cpp
        core/src/types/symbol_entry.cpp
        core/src/utils/log.cpp
        core/src/utils/utils.cpp
        core/src/utils/version.cpp
        core/src/writer.cpp
        core/src/accessor.cpp
        core/src/writer/empty_section.cpp
        core/src/writer/relocation.cpp
        core/src/writer/relocation_section.cpp
        core/src/writer/section.cpp
        core/src/writer/string_section.cpp
        core/src/writer/symbol.cpp
        core/src/writer/symbol_section.cpp
        hpi_component/src/3720/hpi_3720.cpp
        hpi_component/src/4000/hpi_4000.cpp
        hpi_component/src/common/hpi_common_interface.cpp
        hpi_component/src/common/vpux_hpi.cpp
        loader/src/device_buffer_container.cpp
        loader/src/managed_buffer.cpp
        loader/src/platform.cpp
        loader/src/vpux_loader.cpp
)

install(DIRECTORY "core/include" DESTINATION ".")
install(FILES "hpi_component/include/3720/hpi_3720.hpp" DESTINATION "include")
install(FILES "hpi_component/include/4000/hpi_4000.hpp" DESTINATION "include")
install(FILES "hpi_component/include/common/vpux_hpi.hpp" DESTINATION "include")
install(FILES "hpi_component/include/common/hpi_common_interface.hpp" DESTINATION "include")
install(DIRECTORY "loader/include" DESTINATION ".")

install(TARGETS ${PROJECT_NAME} EXPORT "${PROJECT_NAME}Targets")
install(
    EXPORT "${PROJECT_NAME}Targets"
    FILE "${PROJECT_NAME}Targets.cmake"
    NAMESPACE npu::
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ELFConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${version}
    SOVERSION ${version_major}
    INTERFACE_${PROJECT_NAME}_MAJOR_VERSION ${version_major}
)
# ?
set_property(
    TARGET ${PROJECT_NAME}
    APPEND
    PROPERTY COMPATIBLE_INTERFACE_STRING
    ${PROJECT_NAME}_MAJOR_VERSION
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION "${version}"
    COMPATIBILITY SameMajorVersion
)

export(
    EXPORT ${PROJECT_NAME}Targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake"
    NAMESPACE npu::
)
