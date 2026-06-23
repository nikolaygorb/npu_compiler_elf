//
// Copyright (C) 2023-2025 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

//

#pragma once

#include <assert.h>
#include <cassert>
#include <stdexcept>
#include <vpux_elf/types/elf_structs.hpp>
#include <stdint.h>

enum class ErrorCode : uint32_t {
    ELF_SUCCESS = 0,
    ELF_ERROR_UNKNOWN = 1,                                          // Generic error code for conditions, not covered by other error codes
    ELF_ERROR_ACCESS = 20000,                                       // Error related to accessing the ELF file or its contents
    ELF_ERROR_HEADER = 30000,                                       // Error related to the ELF header
    ELF_ERROR_SECTION = 40000,                                      // Error related to ELF sections
    ELF_ERROR_RELOCATION = 50000,                                   // Error related to ELF relocations
    ELF_ERROR_ALLOCATION = 60000,                                   // Error related to memory allocation
    ELF_ERROR_COMPATIBILITY = 70000,                                // Error related to compatibility issues
    ELF_ERROR_RANGE = 80000,                                        // Error related to range issues (e.g., out of bounds)
    ELF_ERROR_RANGE_SECTION_OVERLAPS_NEXT_SECTION = 80001,
    ELF_ERROR_RANGE_SECTION_DOES_NOT_FIT_IN_FILE = 80002,
    ELF_ERROR_RANGE_SECTION_OFFSET_GREATER_THAN_FILE = 80003,
    ELF_ERROR_RANGE_SECTION_READ_GOES_OVER_END_OF_FILE = 80004,
    ELF_ERROR_SEQUENCE = 90000,                                     // Error related to sequence issues (e.g., unexpected order of operations)
    ELF_ERROR_ARGUMENTS = 100000,                                   // Error related to invalid arguments or parameters
    ELF_ERROR_IMPLAUSIBLE_STATE = 110000,                           // Error related to implausible or inconsistent state in the program
};


namespace elf {

// Template exception that just forwards to std::runtime_error or std::logic_error
template<class T, ErrorCode DefaultError = ErrorCode::ELF_ERROR_UNKNOWN>
class TypedException : public T {
public:
    ErrorCode error_code = DefaultError;

    // Constructor for macro usage: message first, then explicit ErrorCode.
    explicit TypedException(const char* what, ErrorCode error_code)
        : T(what)
        , error_code(error_code)
    {}

    // Constructor for macro usage: just message, default ErrorCode is used.
    explicit TypedException(const char* what)
        : T(what)
    {}
};

// Short aliases using the actual exception types directly
using RuntimeError = TypedException<std::runtime_error>;
using LogicError   = TypedException<std::logic_error>;

using AccessError = TypedException<std::runtime_error, ErrorCode::ELF_ERROR_ACCESS>;
using HeaderError = TypedException<std::runtime_error, ErrorCode::ELF_ERROR_HEADER>;
using SectionError = TypedException<std::runtime_error, ErrorCode::ELF_ERROR_SECTION>;
using RelocError = TypedException<std::runtime_error, ErrorCode::ELF_ERROR_RELOCATION>;
using AllocError = TypedException<std::runtime_error, ErrorCode::ELF_ERROR_ALLOCATION>;
using CompatibilityError = TypedException<std::runtime_error, ErrorCode::ELF_ERROR_COMPATIBILITY>;

using RangeError = TypedException<std::logic_error, ErrorCode::ELF_ERROR_RANGE>;
using SequenceError = TypedException<std::logic_error, ErrorCode::ELF_ERROR_SEQUENCE>;
using ArgsError = TypedException<std::logic_error, ErrorCode::ELF_ERROR_ARGUMENTS>;
using ImplausibleState = TypedException<std::logic_error, ErrorCode::ELF_ERROR_IMPLAUSIBLE_STATE>;

#ifdef VPUX_ELF_NOEXCEPT
#define VPUX_ELF_THROW(exception, msg, ...) assert(!(msg))
#else
#define VPUX_ELF_THROW(exception, msg, ...) throw(exception(msg, ##__VA_ARGS__))
#endif

#define VPUX_ELF_THROW_UNLESS(condition, exception, msg, ...) \
    do {                                                      \
        if (!(condition)) {                                   \
            VPUX_ELF_THROW(exception, (msg), ##__VA_ARGS__);  \
        }                                                     \
    } while (0)

#define VPUX_ELF_THROW_WHEN(condition, exception, msg, ...)  \
    do {                                                     \
        if ((condition)) {                                   \
            VPUX_ELF_THROW(exception, (msg), ##__VA_ARGS__); \
        }                                                    \
    } while (0)

}  // namespace elf
