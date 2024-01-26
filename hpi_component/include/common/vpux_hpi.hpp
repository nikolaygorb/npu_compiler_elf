//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <memory>
#include <vector>
#include <vpux_elf/accessor.hpp>
#include <vpux_headers/buffer_manager.hpp>
#include <vpux_headers/device_buffer.hpp>
#include <vpux_headers/metadata.hpp>
#include <vpux_elf/utils/version.hpp>

namespace elf {

namespace platform {

// Clone of Compiler VPU-dialect enum-attribute
enum class ArchKind : uint64_t {
  UNKNOWN = 0,
  VPUX30XX = 1,
  VPUX37XX = 3,
  VPUX40XX = 4,
  VPUX50XX = 5,
};

const std::unordered_map<std::string,
                        elf::platform::ArchKind> knownArch = {{"UNKNOWN", elf::platform::ArchKind::UNKNOWN},
                                                    {"VPUX30XX", elf::platform::ArchKind::VPUX30XX},
                                                    {"VPUX37XX", elf::platform::ArchKind::VPUX37XX},
                                                    {"VPUX40XX", elf::platform::ArchKind::VPUX40XX},
                                                    {"VPUX50XX", elf::platform::ArchKind::VPUX50XX}};

elf::platform::ArchKind mapArchStringToArchKind(const std::string& archName) {
    auto retArch = knownArch.find(archName);
    if (retArch != knownArch.end()) {
        return retArch->second;
    } else {
        return elf::platform::ArchKind::UNKNOWN;
    }
}

std::string stringifyArchKind(elf::platform::ArchKind arch) {
    for (auto archIt : knownArch) {
        if (archIt.second == arch) {
            return archIt.first;
        }
    }
    return std::string("UNKNOWN");
}

// Expected metadata arch info format: "{ARCH}_{REVISION}"
// This funtion parses the string formatted as above an returns
// a std::pair of {arch_string, revision_string}
std::pair<std::string, std::string> parseMetadataArchInfo(std::string metaArchName) {
    auto delimiterLoc = metaArchName.find_first_of("_");
    if (delimiterLoc != std::string::npos) {
        VPUX_ELF_THROW_UNLESS(delimiterLoc == metaArchName.find_last_of("_"), RuntimeError, "ELF Metadata Arch Information not correctly formatted.");
        auto archName = metaArchName.substr(0, delimiterLoc);
        auto revisionName = metaArchName.substr(delimiterLoc + 1);
        return {archName, revisionName};
    } else {
        return {metaArchName, ""};
    }
}

} //namespace platform

class VPUXLoader;

// Structure that gathers configuration options for HPI instances.
// Subject to various additions/modifications in the future
struct HPIConfigs {
    elf::Version nnVersion = elf::Version(0, 0, 0);
    elf::platform::ArchKind archKind = elf::platform::ArchKind::UNKNOWN;
    std::string archRevision = "";
};

class HostParsedInference final {
public:
    HostParsedInference(BufferManager* bufferMgr, AccessManager* accessMgr, elf::HPIConfigs hpiConfigs);
    HostParsedInference(const HostParsedInference& other);
    HostParsedInference(HostParsedInference&& other);
    ~HostParsedInference();

    HostParsedInference& operator=(const HostParsedInference& rhs);
    HostParsedInference& operator=(HostParsedInference&& rhs);

    DeviceBuffer getParsedInference() const;
    std::vector<DeviceBuffer> getAllocatedBuffers() const;
    std::vector<DeviceBuffer> getInputBuffers() const;
    std::vector<DeviceBuffer> getOutputBuffers() const;
    std::vector<DeviceBuffer> getProfBuffers() const;
    std::shared_ptr<const NetworkMetadata> getMetadata();
    const elf::Version getElfABIVersion() const;
    const elf::Version getMIVersion() const;

    void applyInputOutput(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs,
                          std::vector<DeviceBuffer>& profiling);
    void load();

private:
    BufferManager* bufferManager;
    AccessManager* accessManager;
    std::shared_ptr<NetworkMetadata> metadata;
    std::unique_ptr<VPUXLoader> loader;
    std::shared_ptr<AllocatedDeviceBuffer> parsedInference;

    // helpers
    void readMetadata();
    const uint64_t* readPerfMetrics();
    const elf::Version readVersioningInfo(uint32_t versionType) const;
};

}  // namespace elf
