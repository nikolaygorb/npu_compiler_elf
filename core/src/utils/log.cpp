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

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <vpux_elf/utils/log.hpp>

namespace elf {

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

constexpr const char logHeader[LAST][30] = {
        ANSI_COLOR_RED "F:",  ANSI_COLOR_MAGENTA "E:", ANSI_COLOR_YELLOW "W:",
        ANSI_COLOR_CYAN "I:", ANSI_COLOR_BLUE "T:",    ANSI_COLOR_GREEN "D:",
};

LogLevel Logger::globalLevel = ERROR;

void Logger::logprintf(LogLevel lvl, const char* func, const int line, const char* format, ...) {
    [[maybe_unused]] const char headerFormat[] = "%s [%10" PRId64 "] %s:%d\t";
    [[maybe_unused]] uint64_t timestamp = 0;
    va_list args = {};
    va_start(args, format);

    fprintf(stdout, headerFormat, logHeader[lvl], timestamp, func, line);
    vfprintf(stdout, format, args);
    fprintf(stdout, "%s\n", ANSI_COLOR_RESET);

    va_end(args);
}

}  // namespace elf
