
#pragma once

#include <memory>
#include <vpux_loader/vpux_loader.hpp>

namespace elf {

class HostParsedInference final {
public:
    HostParsedInference(BufferManager *bufferMgr, AccessManager *accessMgr);
    ~HostParsedInference();

    DeviceBuffer getParsedInference() const;
    ArrayRef<DeviceBuffer> getAllocatedBuffers() const;
    ArrayRef<DeviceBuffer> getInputBuffers() const;
    ArrayRef<DeviceBuffer> getOutputBuffers() const;
    ArrayRef<DeviceBuffer> getProfBuffers() const;
    NetworkMetadata getMetadata();
    void applyInputOutput(std::vector<DeviceBuffer> &inputs, std::vector<DeviceBuffer> &outputs,
                          std::vector<DeviceBuffer> &profiling);

private:
    DeviceBuffer parsedInference;
    BufferManager *bufferManager;
    ResourceRequirements resRequirements;
    std::unique_ptr<VPUXLoader> loader;
};

} // namespace elf
