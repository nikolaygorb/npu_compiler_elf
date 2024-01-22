//
// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#include <sstream>
#include <vpux_elf/utils/log.hpp>
#include <vpux_elf/utils/version.hpp>

namespace elf {

std::string stringifyVersionTypeEnum(VersionType val) {
    switch (val) {
        case VersionType::UNKNOWN_VERSION: return "UNKNOWN_VERSION";
        case VersionType::ELF_ABI_VERSION: return "ELF_ABI_VERSION";
        case VersionType::MAPPED_INFERENCE_VERSION: return "MAPPED_INFERENCE_VERSION";
        default: return "";
    }
}

void ElfVersion::checkVersionCompatibility(const ElfVersion& expectedVersion, const ElfVersion& recievedVersion, const VersionType versionType) {
    auto versionTypeString = elf::stringifyVersionTypeEnum(versionType);

    std::ostringstream logBuffer;
    if (expectedVersion.major != recievedVersion.major || expectedVersion.minor < recievedVersion.minor) {
        logBuffer << versionTypeString << " is NOT compatible with the ELF";
        VPUX_ELF_LOG(LogLevel::LOG_ERROR, logBuffer.str().c_str());

        logBuffer.str("");
        logBuffer << versionTypeString << " ERROR. Expected: " << expectedVersion << " vs received: " << recievedVersion;
        VPUX_ELF_LOG(LogLevel::LOG_ERROR, logBuffer.str().c_str());
        VPUX_ELF_THROW(VersioningError, logBuffer.str().c_str(), recievedVersion, expectedVersion);
    } else if (expectedVersion.minor > recievedVersion.minor) {
        logBuffer << "Warning! " << versionTypeString << " are compatible but do not fully match.";
        VPUX_ELF_LOG(LogLevel::LOG_WARN, logBuffer.str().c_str());

        logBuffer.str("");
        logBuffer << versionTypeString << " Warning. Expected: " << expectedVersion << " vs received: " << recievedVersion;
        VPUX_ELF_LOG(LogLevel::LOG_WARN, logBuffer.str().c_str());
    } else {
        logBuffer << versionTypeString << " are perfectly compatible. Version: " << expectedVersion;
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, logBuffer.str().c_str());
    }
}

}  // namespace elf
