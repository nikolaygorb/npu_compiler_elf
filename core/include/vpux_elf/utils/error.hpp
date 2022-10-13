//
// Copyright Intel Corporation.
//
// LEGAL NOTICE: Your use of this software and any required dependent software
// (the "Software Package") is subject to the terms and conditions of
// the Intel(R) OpenVINO(TM) Distribution License for the Software Package,
// which may also include notices, disclaimers, or license terms for
// third party or open source software included in or with the Software Package,
// and your use indicates your acceptance of all such terms. Please refer
// to the "third-party-programs.txt" or other similarly-named text file
// included with the Software Package for additional details.
//

#pragma once

#include <assert.h>
#include <cassert>
#include <stdexcept>

namespace elf {

class RuntimeError : public std::runtime_error {
public:
    explicit RuntimeError(const char* what): std::runtime_error(what) {
    }
};

class LogicError : public std::logic_error {
public:
    explicit LogicError(const char* what): std::logic_error(what) {
    }
};

#define VPUX_ELF_DEFINE_EXCEPTION(type, name)         \
    class name : public type {                        \
    public:                                           \
        explicit name(const char* what): type(what) { \
        }                                             \
    }

VPUX_ELF_DEFINE_EXCEPTION(RuntimeError, AccessError);
VPUX_ELF_DEFINE_EXCEPTION(RuntimeError, HeaderError);
VPUX_ELF_DEFINE_EXCEPTION(RuntimeError, SectionError);
VPUX_ELF_DEFINE_EXCEPTION(RuntimeError, RelocError);
VPUX_ELF_DEFINE_EXCEPTION(RuntimeError, AllocError);

VPUX_ELF_DEFINE_EXCEPTION(LogicError, RangeError);
VPUX_ELF_DEFINE_EXCEPTION(LogicError, SequenceError);
VPUX_ELF_DEFINE_EXCEPTION(LogicError, ArgsError);
VPUX_ELF_DEFINE_EXCEPTION(LogicError, ImplausibleState);

#ifdef VPUX_ELF_NOEXCEPT
#define VPUX_ELF_THROW(exception, msg) assert(!(msg))
#else
#define VPUX_ELF_THROW(exception, msg) throw(exception(msg))
#endif

#define VPUX_ELF_THROW_UNLESS(condition, exception, msg) \
    do {                                                 \
        if (!(condition)) {                              \
            VPUX_ELF_THROW((exception), (msg));          \
        }                                                \
    } while (0);

#define VPUX_ELF_THROW_WHEN(condition, exception, msg) \
    do {                                                 \
        if ((condition)) {                               \
            VPUX_ELF_THROW((exception), (msg));          \
        }                                                \
    } while (0);

}  // namespace elf
