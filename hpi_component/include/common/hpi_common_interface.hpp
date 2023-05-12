//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <vpux_loader/vpux_loader.hpp>

namespace elf {

constexpr auto DEFAULT_ALIGN = 64;

class HostParsedInferenceCommon {
public:
    virtual ~HostParsedInferenceCommon() = default;
    virtual ArrayRef<SymbolEntry> getSymTab(uint8_t index) const = 0;

    /**
     * Allocate specific architecture host parsed inference
     *
     * @param bufferManager the buffer manager used by the caller to allocate
     * loader and sections
     *
     * @return DeviceBuffer to the HostParsedInference allocated using
     * the structures used for the current architecture.
     *
     */
    virtual DeviceBuffer allocHostParsedInference(BufferManager *bufferManager) = 0;
    /**
     * Set the entry (mapped inference) and the resource requirements
     * for the pre-alocated DeviceBuffer that contains the current architecture
     * structure in memory
     *
     * @param devBuffer reference to the device buffer in order to reinterpret
     * it to the current architecture specific HPI strucutre
     *
     * @param mapped_entry uint64_t that holds the address of the mapped entry, returned by the loader
     * getEntry method
     *
     * @param resReq resource requirements to be added to the host parsed inference
     */
    virtual void setHostParsedInference(DeviceBuffer &devBuffer, const uint64_t mapped_entry, ResourceRequirements resReq) = 0;

};
} // namespace elf
