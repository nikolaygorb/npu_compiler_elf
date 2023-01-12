# INTEL CONFIDENTIAL
# Copyright 2021 Intel Corporation All Rights Reserved.
#
# The source code contained or described herein and all documents related
# to the source code ("Material") are owned by Intel Corporation or its
# suppliers or licensors. Title to the Material remains with Intel Corporation
# or its suppliers and licensors. The Material contains trade secrets and
# proprietary and confidential information of Intel or its suppliers and
# licensors. The Material is protected by worldwide copyright and trade secret
# laws and treaty provisions. No part of the Material may be used, copied,
# reproduced, modified, published, uploaded, posted, transmitted, distributed,
# or disclosed in any way without Intel's prior express written permission.

# No license under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or delivery
# of the Materials, either expressly, by implication, inducement, estoppel or
# otherwise. Any license under such intellectual property rights must be express
# and approved by Intel in writing.

if(MSVC)
    # Compiler flags

    list(
        APPEND
        UMD_COMPILER_OPTIONS_COMMON
        "/std:c++17"    # ISO C++ programming language standard features
        "/Zm400"        # AdditionalOptions - Increase virtual memory size
        "/MP"           # MultiProcessorCompilation true
        "/W3"           # WarningLevel Level3
        "/WX"           # TreatWarningAsError true
        "/Zi"           # DebugInformationFormat ProgramDatabase
        "/EHsc"         # ExceptionHandling Sync
        "/ZH:SHA_256"   # Enables secure source code hashing, which will be
                        # default in VS2022, so we'll remove this when we upgrade
    )

    if( "${ARCH}" STREQUAL "32" )
    list(
        APPEND UMD_COMPILER_OPTIONS_COMMON
        /arch:SSE2  # Streaming SIMD Extensions 2
        /Gr         # Calling Convention: __fastcall
    )
    endif()

    list(
        APPEND
        UMD_COMPILER_OPTIONS_RELEASE
        "/Ox"  # Full Optimization
        "/Ob2" # Inline Function Expansion: Any Suitable
        "/Oi"  # Enable Instrinsic Functions
        "/Ot"  # Favor speed over size of EXEs and DLLs
        "/GF"  # Enable String Pooling: Yes
        "/MD"  # Runtime Library: Multi-threaded
    )

    list(
        APPEND
        UMD_COMPILER_OPTIONS_DEBUG
        "/GS"  # BufferSecurityCheck
        "/Od"  # OptimizationDisabled
        "/sdl" # SDLchecks
        "/MDd" # Runtime Library: Multi-threaded Debug
        "/bigobj" # increase max .obj sections to 2^32 (from 2^16).
                # avoids: https://learn.microsoft.com/en-us/cpp/error-messages/compiler-errors-1/fatal-error-c1128?view=msvc-170
    )

    set( UMD_COMPILER_OPTIONS_RELEASE ${UMD_COMPILER_OPTIONS_COMMON} ${UMD_COMPILER_OPTIONS_RELEASE} )
    set( UMD_COMPILER_OPTIONS_DEBUG ${UMD_COMPILER_OPTIONS_COMMON} ${UMD_COMPILER_OPTIONS_DEBUG} )

    # Linker flags

    list(
        APPEND
        UMD_LINKER_OPTIONS_COMMON
        "/INCREMENTAL:NO"       # Disable Incremental Linking
        "/MANIFEST:NO"          # Don't create a side-by-side manifest file
        "/MANIFESTUAC:NO"       # Don't embed User Account Control (UAC) information in manifest
        "d3d10_1.lib"
        "/OPT:REF"              # OptimizeReferences
        "/ignore:4099"          # Link library regardless of missing pdb
        "/CETCOMPAT"            # CET Shadow Stack compatible (/CETCOMPAT)
    )

    list(
        APPEND
        UMD_LINKER_OPTIONS_RELEASE
        "/OPT:ICF"              # Enable COMDAT Folding
        "/RELEASE"              # Set the Checksum in the header of .exe file
    )

    list(
        APPEND
        UMD_LINKER_OPTIONS_DEBUG
        "/DEBUG:FULL"           # GenerateDebugInfo
        "/OPT:NOICF"            # Prevent unreferenced data (COMDATs) from being folded in the program
    )

    set( UMD_LINKER_OPTIONS_RELEASE ${UMD_LINKER_OPTIONS_COMMON} ${UMD_LINKER_OPTIONS_RELEASE} )
    set( UMD_LINKER_OPTIONS_DEBUG ${UMD_LINKER_OPTIONS_COMMON} ${UMD_LINKER_OPTIONS_DEBUG} )
endif()
