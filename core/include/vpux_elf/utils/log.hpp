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

#ifndef VPUX_ELF_LOG_ENABLED
#define VPUX_ELF_LOG_ENABLED 1
#endif

#ifndef VPUX_ELF_LOG_UNIT_NAME
#define VPUX_ELF_LOG_UNIT_NAME "unnamed"
#endif

namespace elf {
enum class LogLevel : unsigned int {
    FATAL = 0U,
    ERROR,
    WARN,
    INFO,
    TRACE,
    DEBUG,
    LAST,
};

class Logger {
public:
    explicit Logger(const LogLevel& unitLevel, const char* unitName);
    void logprintf(const LogLevel& level, const char* func, const int line, const char* format, ...);
    static LogLevel getGlobalLevel();
    static void setGlobalLevel(const LogLevel& level);
    LogLevel getUnitLevel(void);
    void setUnitLevel(const LogLevel& level);

private:
    static LogLevel globalLevel;
    LogLevel unitLevel;
    const char* unitName;

    static void updateLevelVar(LogLevel& levelVar, const LogLevel& levelVal);
};

static constexpr char unitName[] = VPUX_ELF_LOG_UNIT_NAME;
static Logger unitLogger(LogLevel::ERROR, VPUX_ELF_LOG_UNIT_NAME);

#ifndef VPUX_ELF_LOG
#define VPUX_ELF_LOG(lvl, ...)                                                                                         \
    do {                                                                                                               \
        if ((VPUX_ELF_LOG_ENABLED) && (((lvl) <= Logger::getGlobalLevel()) || ((lvl) <= unitLogger.getUnitLevel()))) { \
            unitLogger.logprintf(lvl, __func__, __LINE__, ##__VA_ARGS__);                                              \
        }                                                                                                              \
    } while (0);
#endif

}  // namespace elf
