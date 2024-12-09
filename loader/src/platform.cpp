//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#include <unordered_map>
#include <vpux_headers/platform.hpp>

namespace elf {

namespace platform {


const std::unordered_map<std::string, elf::platform::ArchKind>& getKnownArchitectures() {
    static const std::unordered_map<std::string, elf::platform::ArchKind> knownArch = {
#ifdef INTEL_EMBARGO_NPU5
                                                    {"VPUX50XX", elf::platform::ArchKind::VPUX50XX},
#endif  // INTEL_EMBARGO_NPU5
#ifdef INTEL_EMBARGO_NPU6
                                                    {"VPUX60XX", elf::platform::ArchKind::VPUX60XX},
#endif  // INTEL_EMBARGO_NPU6
                                                    {"UNKNOWN", elf::platform::ArchKind::UNKNOWN},
                                                    {"VPUX30XX", elf::platform::ArchKind::VPUX30XX},
                                                    {"VPUX37XX", elf::platform::ArchKind::VPUX37XX},
                                                    {"VPUX40XX", elf::platform::ArchKind::VPUX40XX}
    };

    return knownArch;
}

elf::platform::ArchKind mapArchStringToArchKind(const std::string& archName) {
    auto& knownArch = getKnownArchitectures();
    auto retArch = knownArch.find(archName);
    if (retArch != knownArch.end()) {
        return retArch->second;
    } else {
        return elf::platform::ArchKind::UNKNOWN;
    }
}

std::string stringifyArchKind(const elf::platform::ArchKind& arch) {
    auto& knownArch = getKnownArchitectures();
    for (auto archIt : knownArch) {
        if (archIt.second == arch) {
            return archIt.first;
        }
    }
    return std::string("UNKNOWN");
}

uint8_t getHardwareTileCount(const elf::platform::ArchKind& arch) {
    // map between archKind and maximum hardware tile count
    static const std::unordered_map<elf::platform::ArchKind, uint8_t> hardwareTileCountsMap = {
#ifdef INTEL_EMBARGO_NPU5
        {elf::platform::ArchKind::VPUX50XX, 4},
#endif  // INTEL_EMBARGO_NPU5
#ifdef INTEL_EMBARGO_NPU6
        {elf::platform::ArchKind::VPUX60XX, 4},
#endif  // INTEL_EMBARGO_NPU6
        {elf::platform::ArchKind::UNKNOWN, 0},
        {elf::platform::ArchKind::VPUX30XX, 2},
        {elf::platform::ArchKind::VPUX37XX, 2},
        {elf::platform::ArchKind::VPUX40XX, 6}

    };
    // get maximum hardware tile count, archKind has already been checked before
    return hardwareTileCountsMap.find(arch)->second;
}

} // namespace platform

} // namespace elf
