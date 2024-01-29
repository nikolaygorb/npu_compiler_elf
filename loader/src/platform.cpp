//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#include <unordered_map>
#include <vpux_headers/platform.hpp>

namespace elf {

namespace platform {

namespace {
const std::unordered_map<std::string, elf::platform::ArchKind> knownArch = {{"UNKNOWN", elf::platform::ArchKind::UNKNOWN},
                                                    {"VPUX30XX", elf::platform::ArchKind::VPUX30XX},
                                                    {"VPUX37XX", elf::platform::ArchKind::VPUX37XX},
                                                    {"VPUX40XX", elf::platform::ArchKind::VPUX40XX},
                                                    {"VPUX50XX", elf::platform::ArchKind::VPUX50XX}};
} // namespace 

elf::platform::ArchKind mapArchStringToArchKind(const std::string& archName) {
    auto retArch = knownArch.find(archName);
    if (retArch != knownArch.end()) {
        return retArch->second;
    } else {
        return elf::platform::ArchKind::UNKNOWN;
    }
}

std::string stringifyArchKind(const elf::platform::ArchKind& arch) {
    for (auto archIt : knownArch) {
        if (archIt.second == arch) {
            return archIt.first;
        }
    }
    return std::string("UNKNOWN");
}

} // namespace platform

} // namespace elf