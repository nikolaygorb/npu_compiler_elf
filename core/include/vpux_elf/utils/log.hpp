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

#ifndef VPUX_ELF_ENABLE_LOGGING
#define VPUX_ELF_ENABLE_LOGGING 1
#endif

namespace elf {
enum LogLevel {
    FATAL = 0,
    ERROR,
    WARN,
    INFO,
    TRACE,
    DEBUG,
    LAST,
};

class Logger {
public:
    explicit Logger(LogLevel unitLevel): unitLevel(unitLevel) {
    }

    static void logprintf(LogLevel lvl, const char* func, const int line, const char* format, ...);

    inline static LogLevel getGlobalLevel(void) {
        return globalLevel;
    }

    inline static void setGlobalLevel(const LogLevel& newLevel) {
        updateLevelVar(globalLevel, newLevel);
    }

    inline LogLevel getUnitLevel(void) {
        return unitLevel;
    }

    inline void setUnitLevel(const LogLevel& newLevel) {
        updateLevelVar(unitLevel, newLevel);
    }

private:
    inline static void updateLevelVar(LogLevel& Var, LogLevel newLevel) {
        if (LAST <= newLevel) {
            return;
        } else {
            Var = newLevel;
        }
    }

    static LogLevel globalLevel;
    LogLevel unitLevel;
};

static Logger unitLogger(ERROR);

#ifndef VPUX_ELF_LOG
#define VPUX_ELF_LOG(lvl, ...)                                                                                        \
    do {                                                                                                              \
        if ((VPUX_ELF_ENABLE_LOGGING) && ((lvl <= Logger::getGlobalLevel()) || (lvl <= unitLogger.getUnitLevel()))) { \
            Logger::logprintf(lvl, __func__, __LINE__, ##__VA_ARGS__);                                                \
        }                                                                                                             \
    } while (0);
#endif

}  // namespace elf
