//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <vpux_headers/serial_struct_base.hpp>

namespace elf {

namespace platform {

// Common string size used by the drivers
// Also defined in metadata_primitives.hpp. To be moved to common primitives header
constexpr uint16_t MAX_STRING_LEN = 256;
using BasicString = char[MAX_STRING_LEN];

enum class ArchKind : uint64_t {
    UNKNOWN = 0,
    VPUX30XX = 1,
    VPUX37XX = 3,
    VPUX40XX = 4,
    VPUX50XX = 5,
};

elf::platform::ArchKind mapArchStringToArchKind(const std::string& archName);
std::string stringifyArchKind(const elf::platform::ArchKind& arch);

// Need a wrapper over the string in order for the serialization infrastructure to accept it
// Can also be extended in the future.
struct RevisionInfo {
    BasicString revision;
};

struct PlatformInfo {
    ArchKind mArchKind;
    RevisionInfo mArchRevision;
};

static_assert(sizeof(PlatformInfo) == 264, "PlatformInfo size != 264");

class SerialPlatformInfo : public elf::SerialStructBase {
public:
    SerialPlatformInfo(PlatformInfo& platformInfo) {
        addElement(platformInfo.mArchKind);
        addElement(platformInfo.mArchRevision);
    }
};

class PlatformInfoSerialization {
public:
    static std::vector<uint8_t> serialize(PlatformInfo& platformInfo) {
        return SerialPlatformInfo(platformInfo).serialize();
    }

    static const std::shared_ptr<PlatformInfo> deserialize(const uint8_t* buffer, uint64_t size) {
        const auto platformInfo = std::make_shared<PlatformInfo>();
        SerialPlatformInfo(*platformInfo).deserialize(buffer, size);

        return platformInfo;
    }
};

}  // namespace platform

}  // namespace elf
