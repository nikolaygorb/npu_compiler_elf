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

#ifndef __VPUX_ELF_LOG_H__
#define __VPUX_ELF_LOG_H__

#ifndef VPUX_ELF_ENABLE_LOGGING
#define VPUX_ELF_ENABLE_LOGGING 1
#endif

#include "log_helpers.hpp"

#define VPUX_ELF_FATAL_LEVEL 0
#define VPUX_ELF_ERROR_LEVEL 1
#define VPUX_ELF_WARN_LEVEL  2
#define VPUX_ELF_INFO_LEVEL  3
#define VPUX_ELF_TRACE_LEVEL 4
#define VPUX_ELF_DEBUG_LEVEL 5

#define VPUX_ELF_LOG_DEFAULT_LEVEL VPUX_ELF_WARN_LEVEL

#ifndef VPUX_ELF_LOG_LEVEL
#define VPUX_ELF_LOG_LEVEL VPUX_ELF_LOG_DEFAULT_LEVEL
#endif

#define vpuxElfLogFunc(__ELF_LOG_LEVEL__, ...) vpuxLog(__ELF_LOG_LEVEL__, __VA_ARGS__)

#define vpuxElfLog(LOG_TYPE, ...)                                               \
    do {                                                                        \
        constexpr bool elf_showlog___ = LOG_TYPE##_LEVEL <= VPUX_ELF_LOG_LEVEL; \
        if (elf_showlog___ && VPUX_ELF_ENABLE_LOGGING) {                        \
            vpuxElfLogFunc(LOG_TYPE, ##__VA_ARGS__);                            \
        }                                                                       \
    } while (0)

#endif // __VPUX_ELF_LOG_H__
