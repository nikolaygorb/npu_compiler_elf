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

#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#ifndef VPUX_ELF_ENABLE_LOGGING
#define VPUX_ELF_ENABLE_LOGGING 1
#endif

namespace elf
{

// Windows-only
#if (defined (WINNT) || defined(_WIN32) || defined(_WIN64) )
#define __attribute__(x)
#endif

#define VPUX_ELF_LOG_ANSI_COLOR_RED     "\x1b[31m"
#define VPUX_ELF_LOG_ANSI_COLOR_GREEN   "\x1b[32m"
#define VPUX_ELF_LOG_ANSI_COLOR_YELLOW  "\x1b[33m"
#define VPUX_ELF_LOG_ANSI_COLOR_BLUE    "\x1b[34m"
#define VPUX_ELF_LOG_ANSI_COLOR_MAGENTA "\x1b[35m"
#define VPUX_ELF_LOG_ANSI_COLOR_CYAN    "\x1b[36m"
#define VPUX_ELF_LOG_ANSI_COLOR_WHITE   "\x1b[37m"
#define VPUX_ELF_LOG_ANSI_COLOR_RESET   "\x1b[0m"

#ifndef VPUX_ELF_LOG_FATAL_COLOR
#define VPUX_ELF_LOG_FATAL_COLOR VPUX_ELF_LOG_ANSI_COLOR_RED
#endif

#ifndef VPUX_ELF_LOG_ERROR_COLOR
#define VPUX_ELF_LOG_ERROR_COLOR VPUX_ELF_LOG_ANSI_COLOR_MAGENTA
#endif

#ifndef VPUX_ELF_LOG_WARN_COLOR
#define VPUX_ELF_LOG_WARN_COLOR VPUX_ELF_LOG_ANSI_COLOR_YELLOW
#endif

#ifndef VPUX_ELF_LOG_INFO_COLOR
#define VPUX_ELF_LOG_INFO_COLOR VPUX_ELF_LOG_ANSI_COLOR_CYAN
#endif

#ifndef VPUX_ELF_LOG_TRACE_COLOR
#define VPUX_ELF_LOG_TRACE_COLOR VPUX_ELF_LOG_ANSI_COLOR_BLUE
#endif

#ifndef VPUX_ELF_LOG_DEBUG_COLOR
#define VPUX_ELF_LOG_DEBUG_COLOR VPUX_ELF_LOG_ANSI_COLOR_GREEN
#endif

typedef enum LogLevel {
    VPUX_ELF_LOG_FATAL = 0,
    VPUX_ELF_LOG_ERROR,
    VPUX_ELF_LOG_WARN,
    VPUX_ELF_LOG_INFO,
    VPUX_ELF_LOG_TRACE,
    VPUX_ELF_LOG_DEBUG,
    VPUX_ELF_LOG_LAST,
} LogLevel;

static const char logHeader[VPUX_ELF_LOG_LAST][30] =
{
    VPUX_ELF_LOG_FATAL_COLOR "F:",
    VPUX_ELF_LOG_ERROR_COLOR "E:",
    VPUX_ELF_LOG_WARN_COLOR  "W:",
    VPUX_ELF_LOG_INFO_COLOR  "I:",
    VPUX_ELF_LOG_TRACE_COLOR "T:",
    VPUX_ELF_LOG_DEBUG_COLOR "D:",
};

#ifndef VPUX_ELF_LOG_UNIT_NAME
#define VPUX_ELF_LOG_UNIT_NAME unnamed
#endif

#define _VPUX_ELF_LOG_LEVEL(UNIT_NAME)  vpuxElfLogLevel_ ## UNIT_NAME
#define  VPUX_ELF_LOG_LEVEL(UNIT_NAME) _VPUX_ELF_LOG_LEVEL(UNIT_NAME)

unsigned int VPUX_ELF_LOG_LEVEL(VPUX_ELF_LOG_UNIT_NAME) = VPUX_ELF_LOG_ERROR;
extern unsigned int VPUX_ELF_LOG_LEVEL(default);

// Set log level for the current unit.
#define VPUX_ELF_LOG_LEVEL_SET(lvl) if (lvl < VPUX_ELF_LAST) { VPUX_ELF_LOG_LEVEL(VPUX_ELF_UNIT_NAME) = lvl; }

// Set the global log level. Can be used to prevent modules from hiding messages (enable all of them with a single change)
// This should be an application setting, not a per module one.
#define VPUX_ELF_LOG_DEFAULT_LEVEL_SET(lvl) if (lvl < VPUX_ELF_LOG_LAST) { VPUX_ELF_LOG_LEVEL(default) = lvl; }

static int __attribute__((unused))
logprintf(enum LogLevel lvl, const char * func __attribute__((unused)),
          const int line __attribute__((unused)),
          const char * format, ...)
{
    if(lvl > VPUX_ELF_LOG_LEVEL(VPUX_ELF_LOG_UNIT_NAME) &&
       lvl > VPUX_ELF_LOG_LEVEL(default))
        return 0;

    [[maybe_unused]] const char headerFormat[] = "%s [%10" PRId64 "] %s:%d\t";
    [[maybe_unused]] uint64_t timestamp = 0;
    va_list args = {};
    va_start (args, format);

    fprintf(stdout, headerFormat, logHeader[lvl], timestamp, func, line);
    vfprintf(stdout, format, args);
    fprintf(stdout, "%s\n", VPUX_ELF_LOG_ANSI_COLOR_RESET);
    va_end (args);
    return 0;
}

#define VPUX_ELF_LOG_FUNC(lvl, format, ...)                           \
    logprintf(lvl, __func__, __LINE__, format, ##__VA_ARGS__)

#define VPUX_ELF_LOG(LOG_LEVEL, ...)                \
    if (VPUX_ELF_ENABLE_LOGGING) {               \
        VPUX_ELF_LOG_FUNC(LOG_LEVEL, ##__VA_ARGS__); \
    }

}
