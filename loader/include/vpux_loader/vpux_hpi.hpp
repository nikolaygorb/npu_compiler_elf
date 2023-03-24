
#pragma once

#include <memory>
#include <vpux_loader/vpux_loader.hpp>

namespace elf {

class HostParsedInference final {
public:
    HostParsedInference(BufferManager* bufferMgr, AccessManager* accessMgr);
    ~HostParsedInference();

    DeviceBuffer getParsedInference();
    ArrayRef<DeviceBuffer> getAllocatedBuffers() const;
    ArrayRef<DeviceBuffer> getInputBuffers() const;
    ArrayRef<DeviceBuffer> getOutputBuffers() const;
    NetworkMetadata getMetadata();
    void applyInputOutput(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs);

private:
    DeviceBuffer parsedInference;
    BufferManager* bufferManager;
    ResourceRequirements resRequirements;
    std::vector<std::unique_ptr<VPUXLoader>> loaders;
    mutable std::vector<DeviceBuffer> allocations;
    mutable std::vector<DeviceBuffer> inputs;
    mutable std::vector<DeviceBuffer> outputs;
};

}  // namespace elf
