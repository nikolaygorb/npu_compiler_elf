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

#ifndef VPUX_ELF_LOG_HELPERS_H__
#define VPUX_ELF_LOG_HELPERS_H__

/*
 * Add logging capabilities over simple printf.
 * Before including header, a unit name can be set, otherwise defaults to global. eg:
 *
 * #define VPUX_ELF_UNIT_NAME unitname
 * #include <log.hpp>
 */

#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

// Windows-only
#if (defined (WINNT) || defined(_WIN32) || defined(_WIN64) )
#define __attribute__(x)
#endif

#ifndef VPUX_ELF_UNIT_NAME
#define VPUX_ELF_UNIT_NAME global
#endif

#define _VPUX_ELF_LOGLEVEL(UNIT_NAME)  vpuxElfLogLevel_ ## UNIT_NAME
#define  VPUX_ELF_LOGLEVEL(UNIT_NAME) _VPUX_ELF_LOGLEVEL(UNIT_NAME)

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_WHITE   "\x1b[37m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#ifndef VPUX_ELF_LOG_DEBUG_COLOR
#define VPUX_ELF_LOG_DEBUG_COLOR ANSI_COLOR_WHITE
#endif

#ifndef VPUX_ELF_LOG_INFO_COLOR
#define VPUX_ELF_LOG_INFO_COLOR ANSI_COLOR_CYAN
#endif

#ifndef VPUX_ELF_LOG_TRACE_COLOR
#define VPUX_ELF_LOG_TRACE_COLOR ANSI_COLOR_WHITE
#endif

#ifndef VPUX_ELF_LOG_WARN_COLOR
#define VPUX_ELF_LOG_WARN_COLOR ANSI_COLOR_YELLOW
#endif

#ifndef VPUX_ELF_LOG_ERROR_COLOR
#define VPUX_ELF_LOG_ERROR_COLOR ANSI_COLOR_MAGENTA
#endif

#ifndef VPUX_ELF_LOG_FATAL_COLOR
#define VPUX_ELF_LOG_FATAL_COLOR ANSI_COLOR_RED
#endif

typedef enum vpuxLog_t {
    VPUX_ELF_FATAL = 0,
    VPUX_ELF_ERROR,
    VPUX_ELF_WARN,
    VPUX_ELF_INFO,
    VPUX_ELF_TRACE,
    VPUX_ELF_DEBUG,
    VPUX_ELF_LAST,
} vpuxLog_t;

static const char vpuLogHeader[VPUX_ELF_LAST][30] =
{
    VPUX_ELF_LOG_FATAL_COLOR "F:",
    VPUX_ELF_LOG_ERROR_COLOR "E:",
    VPUX_ELF_LOG_WARN_COLOR  "W:",
    VPUX_ELF_LOG_INFO_COLOR  "I:",
    VPUX_ELF_LOG_TRACE_COLOR "T:",
    VPUX_ELF_LOG_DEBUG_COLOR "D:"
};

unsigned int VPUX_ELF_LOGLEVEL(VPUX_ELF_UNIT_NAME) = VPUX_ELF_INFO;
static unsigned int VPUX_ELF_LOGLEVEL(default) = VPUX_ELF_INFO;

static int __attribute__((unused))
logprintf(enum vpuxLog_t lvl, const char * func __attribute__((unused)),
          const int line __attribute__((unused)),
          const char * format, ...)
{
    if(lvl < VPUX_ELF_LOGLEVEL(VPUX_ELF_UNIT_NAME) &&
       lvl < VPUX_ELF_LOGLEVEL(default))
        return 0;

    [[maybe_unused]] const char headerFormat[] = "%s [%10" PRId64 "] %s:%d\t";
    [[maybe_unused]] uint64_t timestamp = 0;
    va_list args = {};
    va_start (args, format);

    fprintf(stdout, headerFormat, vpuLogHeader[lvl], timestamp, func, line);
    vfprintf(stdout, format, args);
    fprintf(stdout, "%s\n", ANSI_COLOR_RESET);
    va_end (args);
    return 0;
}

#define vpuxLog(lvl, format, ...)                           \
    logprintf(lvl, __func__, __LINE__, format, ##__VA_ARGS__)

// Set log level for the current unit. Note that the level must be smaller than the global default
#define vpuxElfLogLevelSet(lvl) if (lvl < VPUX_ELF_LAST) { VPUX_ELF_LOGLEVEL(VPUX_ELF_UNIT_NAME) = lvl; }

// Set the global log level. Can be used to prevent modules from hiding messages (enable all of them with a single change)
// This should be an application setting, not a per module one
#define vpuxElfLogDefaultLevelSet(lvl) if (lvl < VPUX_ELF_LAST) { VPUX_ELF_LOGLEVEL(default) = lvl; }

#endif // VPUX_ELF_LOG_HELPERS_H__
