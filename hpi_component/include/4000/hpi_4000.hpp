//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <vpux_loader/vpux_loader.hpp>
#include <hpi_common_interface.hpp>

namespace elf {
class HostParsedInference_4000 : public HostParsedInferenceCommon {
public:
    ArrayRef<SymbolEntry> getSymTab(uint8_t index) const override;
    std::string getType() const override;
    DeviceBuffer allocHostParsedInference(BufferManager *bufferManager) override;
    void setHostParsedInference(DeviceBuffer &devBuffer, uint64_t mapped_entry, ResourceRequirements resReq) override;
};
} // namespace elf
