
//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#include <vpux_elf/utils/utils.hpp>
#include <vpux_elf/utils/log.hpp>
#include <vpux_elf/utils/error.hpp>
#include <vpux_elf/types/section_header.hpp>
#include <vpux_elf/types/vpu_extensions.hpp>
#include <vpux_headers/array_ref.hpp>
#include <hpi_4000.hpp>
#include <api/vpu_nnrt_api_40xx.h>
#include <api/vpu_cmx_info_40xx.h>
#include <array>

namespace elf {
// TODO: EISW-79509

namespace {

std::vector<SymbolEntry> symTab_;
std::vector<std::string> stringContainers_;

// Base of frequency values used in tables (in MHz).
constexpr uint32_t FREQ_BASE = 700;
// Step of frequency for each entry in tables (in MHz).
constexpr uint32_t FREQ_STEP = 100;
// Base of bandwidth values used in tables (in MB/s).
constexpr uint32_t BW_BASE = 2000;
// Step of bandwidth values used in tables (in MB/s).
constexpr uint32_t BW_STEP = 100;

// value in [0.0..1.0] range indicating scalability of network for a given DDR bandwidth.
const std::array<float, nn_public::VPU_SCALABILITY_VALUES_PER_FREQ> byBWScales({0.0F, 0.2F, 0.4F, 0.6F, 0.8F});
// expected ticks (based on FRC @37.5MHz) an inference should take for a given DDR bandwidth.
const std::array<uint64_t, nn_public::VPU_SCALABILITY_VALUES_PER_FREQ> byBWTicks({10UL, 12UL, 14UL, 16UL, 18UL});

} // namespace

void setDefaultPerformanceMetrics(nn_public::VpuPerformanceMetrics &metrics) {
    metrics.bw_base = BW_BASE;
    metrics.bw_step = BW_STEP;
    metrics.freq_base = FREQ_BASE;
    metrics.freq_step = FREQ_STEP;

    for (uint32_t i = 0; i < nn_public::VPU_SCALABILITY_NUM_OF_FREQ; ++i) {
        std::copy(byBWScales.begin(), byBWScales.end(), std::begin(metrics.scalability[i]));
        std::copy(byBWTicks.begin(), byBWTicks.end(), std::begin(metrics.ticks[i]));
    }
}

ArrayRef<SymbolEntry> HostParsedInference_4000::getSymbolTable(uint8_t) const {
    uintptr_t metadata = nn_public::VPU_METADATA_STORAGE_ADDR;

    {
        metadata = nn_public::align_storage(alignof(nn_public::VpuDPUInvariant), metadata);

        SymbolEntry dpuInvariantMetadata;
        dpuInvariantMetadata.st_info = static_cast<unsigned char>(elf64STInfo(elf::STB_GLOBAL, elf::STT_OBJECT));
        dpuInvariantMetadata.st_other = STV_DEFAULT;
        dpuInvariantMetadata.st_shndx = 0;
        dpuInvariantMetadata.st_value = reinterpret_cast<uint64_t>(metadata);
        // TODO: What to write as size if amount of task in metadata buffer is defined by compiler?
        // Supposed to be unused? Applies to other symbols below as well
        dpuInvariantMetadata.st_size = 0;
        dpuInvariantMetadata.st_name = 0;

        symTab_.push_back(dpuInvariantMetadata);
        stringContainers_.push_back("program.DPUInvariant.cmx.0.0");

        metadata += nn_public::VPU_INVARIANT_COUNT * sizeof(nn_public::VpuDPUInvariant);
    }

    {
        metadata = nn_public::align_storage(alignof(nn_public::VpuDPUVariant), metadata);

        SymbolEntry dpuVariantMetadata;
        dpuVariantMetadata.st_info = static_cast<unsigned char>(elf64STInfo(elf::STB_GLOBAL, elf::STT_OBJECT));
        dpuVariantMetadata.st_other = STV_DEFAULT;
        dpuVariantMetadata.st_shndx = 0;
        dpuVariantMetadata.st_value = reinterpret_cast<uint64_t>(metadata);
        dpuVariantMetadata.st_size = 0;
        dpuVariantMetadata.st_name = 0;

        symTab_.push_back(dpuVariantMetadata);
        stringContainers_.push_back("program.DPUVariant.cmx.0.0");

        metadata += nn_public::VPU_VARIANT_COUNT * sizeof(nn_public::VpuDPUVariant);
    }

    {
        metadata = nn_public::align_storage(alignof(nn_public::VpuActKernelRange), metadata);

        SymbolEntry actKernelRangeMetadata;
        actKernelRangeMetadata.st_info = static_cast<unsigned char>(elf64STInfo(elf::STB_GLOBAL, elf::STT_OBJECT));
        actKernelRangeMetadata.st_other = STV_DEFAULT;
        actKernelRangeMetadata.st_shndx = 0;
        actKernelRangeMetadata.st_value = reinterpret_cast<uint64_t>(metadata);
        actKernelRangeMetadata.st_size = 0;
        actKernelRangeMetadata.st_name = 0;

        symTab_.push_back(actKernelRangeMetadata);
        stringContainers_.push_back("program.ActKernelRange.cmx.0.0");

        metadata += nn_public::VPU_KERNEL_RANGE_COUNT * sizeof(nn_public::VpuActKernelRange);
    }

    {
        metadata = nn_public::align_storage(alignof(nn_public::VpuActKernelInvocation), metadata);

        SymbolEntry actKernelInvocationMetadata;
        actKernelInvocationMetadata.st_info = static_cast<unsigned char>(elf64STInfo(elf::STB_GLOBAL, elf::STT_OBJECT));
        actKernelInvocationMetadata.st_other = STV_DEFAULT;
        actKernelInvocationMetadata.st_shndx = 0;
        actKernelInvocationMetadata.st_value = reinterpret_cast<uint64_t>(metadata);
        actKernelInvocationMetadata.st_size = 0;
        actKernelInvocationMetadata.st_name = 0;

        symTab_.push_back(actKernelInvocationMetadata);
        stringContainers_.push_back("program.ActKernelInvocation.cmx.0.0");

        metadata += nn_public::VPU_KERNEL_INVO_COUNT * sizeof(nn_public::VpuActKernelInvocation);
    }

    {
        metadata = nn_public::align_storage(alignof(nn_public::VpuDMATask), metadata);

        SymbolEntry dmaDDRMetadata;
        dmaDDRMetadata.st_info = static_cast<unsigned char>(elf64STInfo(elf::STB_GLOBAL, elf::STT_OBJECT));
        dmaDDRMetadata.st_other = STV_DEFAULT;
        dmaDDRMetadata.st_shndx = 0;
        dmaDDRMetadata.st_value = reinterpret_cast<uint64_t>(metadata);
        dmaDDRMetadata.st_size = 0;
        dmaDDRMetadata.st_name = 0;

        symTab_.push_back(dmaDDRMetadata);
        stringContainers_.push_back("program.DMA.cmx.0.0");

        // TODO: short-term solution, 32 is hardcoded in compiler (VPU40XX::MappedInference::serialize) and here
        metadata += 32 * sizeof(nn_public::VpuDMATask);
    }

    {
        metadata = nn_public::align_storage(alignof(nn_public::VpuDMATask), metadata);

        SymbolEntry dmaCMXMetadata;
        dmaCMXMetadata.st_info = static_cast<unsigned char>(elf64STInfo(elf::STB_GLOBAL, elf::STT_OBJECT));
        dmaCMXMetadata.st_other = STV_DEFAULT;
        dmaCMXMetadata.st_shndx = 0;
        dmaCMXMetadata.st_value = reinterpret_cast<uint64_t>(metadata);
        dmaCMXMetadata.st_size = 0;
        dmaCMXMetadata.st_name = 0;

        symTab_.push_back(dmaCMXMetadata);
        stringContainers_.push_back("program.DMA.cmx.0.1");

        // TODO: short-term solution, 32 is hardcoded in compiler (VPU40XX::MappedInference::serialize) and here
        metadata += 32 * sizeof(nn_public::VpuDMATask);
    }

    {
        SymbolEntry cmxWorkspace;
        cmxWorkspace.st_info = static_cast<unsigned char>(elf64STInfo(elf::STB_GLOBAL, elf::STT_OBJECT));
        cmxWorkspace.st_other = STV_DEFAULT;
        cmxWorkspace.st_shndx = 0;
        cmxWorkspace.st_value = nn_public::VPU_WORKSPACE_ADDR;
        cmxWorkspace.st_size = nn_public::VPU_WORKSPACE_SIZE;
        cmxWorkspace.st_name = 0;

        symTab_.push_back(cmxWorkspace);
        stringContainers_.push_back("buffer.CMX_NN.0");
    }

    // For LNL we only have one symtab
    return ArrayRef<SymbolEntry>(symTab_);
}

ArrayRef<std::string> HostParsedInference_4000::getSymbolNames() const {
    return ArrayRef<std::string>(stringContainers_);
}

DeviceBuffer HostParsedInference_4000::allocateHostParsedInference(BufferManager *bufferManager) {
    return bufferManager->allocate(BufferSpecs(
        DEFAULT_ALIGN, utils::alignUp(sizeof(nn_public::VpuHostParsedInference), DEFAULT_ALIGN), SHF_EXECINSTR));
}

void HostParsedInference_4000::setHostParsedInference(DeviceBuffer &devBuffer, uint64_t mapped_entry,
                                                      ResourceRequirements resReq) {
    auto hpi = reinterpret_cast<nn_public::VpuHostParsedInference *>(devBuffer.cpu_addr());

    hpi->resource_requirements_ = {};
    hpi->resource_requirements_.nn_slice_count_ = resReq.nn_slice_count_;
    hpi->resource_requirements_.nn_barriers_ = resReq.nn_barriers_;
    setDefaultPerformanceMetrics(hpi->performance_metrics_);

    hpi->mapped_.address = mapped_entry;
    hpi->mapped_.count = 1;
}

} // namespace elf
