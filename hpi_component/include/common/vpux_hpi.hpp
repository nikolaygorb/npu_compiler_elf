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

class VPUXLoader;

class HostParsedInference final {
public:
    enum class ArchKind { UNKNOWN = 0, VPUX37XX, VPUX40XX, VPUX50XX};

    HostParsedInference(BufferManager* bufferMgr, AccessManager* accessMgr, ArchKind expArchKind);
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
