//
// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

// see https://gcc.gnu.org/wiki/Visibility

#if defined(__GNUC__) && __GNUC__ < 4
    #error "ELF library doesn't support GNU compiler older than ver. 4"
#endif

#if defined(_WIN32)
    #define ELF_HELPER_DLL_IMPORT __declspec(dllimport)
    #define ELF_HELPER_DLL_EXPORT __declspec(dllexport)
    #define ELF_HELPER_DLL_LOCAL
#elif defined(__clang__) || defined(__GNUC__)
    #define ELF_HELPER_DLL_IMPORT __attribute__ ((visibility("default")))
    #define ELF_HELPER_DLL_EXPORT ELF_HELPER_DLL_IMPORT
    #define ELF_HELPER_DLL_LOCAL __attribute__ ((visibility("hidden")))
#else
    #error "ELF library supports only MSVC, GNU and Clang as C/C++ compiler"
#endif

// ELF_EXPORTS defined by CMake automatically upon target creation
#if defined(ELF_EXPORTS)
    #define ELF_API ELF_HELPER_DLL_EXPORT
#else
    #define ELF_API
#endif

#define ELF_LOCAL ELF_HELPER_DLL_LOCAL
