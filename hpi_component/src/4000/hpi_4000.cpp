
#include <vpux_loader/vpux_loader.hpp>
#include <vpux_elf/utils/utils.hpp>
#include <hpi_4000.hpp>
#include "nn_public.h"
#include "vpu_cmx_info_lnl.h"
#include "nce_vpu4_hw.h"
#include <array>

namespace elf {
// TODO: EISW-79509
static constexpr uint8_t N_TABS = 1; // as for now we don't support more than 1 tile
static constexpr size_t SPECIAL_SYMTAB_SIZE = 8;
static SymbolEntry symTab_[N_TABS][SPECIAL_SYMTAB_SIZE];

namespace {
// Base of frequency values used in tables (in MHz).
static constexpr uint32_t FREQ_BASE = 700;
// Step of frequency for each entry in tables (in MHz).
static constexpr uint32_t FREQ_STEP = 100;
// Base of bandwidth values used in tables (in MB/s).
static constexpr uint32_t BW_BASE = 2000;
// Step of bandwidth values used in tables (in MB/s).
static constexpr uint32_t BW_STEP = 100;

// value in [0.0..1.0] range indicating scalability of network for a given DDR bandwidth.
static const std::array<float, nn_public::VPU_SCALABILITY_VALUES_PER_FREQ> byBWScales({0.0F, 0.2F, 0.4F, 0.6F, 0.8F});
// expected ticks (based on FRC @37.5MHz) an inference should take for a given DDR bandwidth.
static const std::array<uint64_t, nn_public::VPU_SCALABILITY_VALUES_PER_FREQ> byBWTicks({10UL, 12UL, 14UL, 16UL, 18UL});

} // namespace

static void setDefaultPerformanceMetrics(nn_public::VpuPerformanceMetrics &metrics) {
    metrics.bw_base = BW_BASE;
    metrics.bw_step = BW_STEP;
    metrics.freq_base = FREQ_BASE;
    metrics.freq_step = FREQ_STEP;

    for (uint32_t i = 0; i < nn_public::VPU_SCALABILITY_NUM_OF_FREQ; ++i) {
        std::copy(byBWScales.begin(), byBWScales.end(), std::begin(metrics.scalability[i]));
        std::copy(byBWTicks.begin(), byBWTicks.end(), std::begin(metrics.ticks[i]));
    }
}

ArrayRef<SymbolEntry> HostParsedInference_4000::getSymbolTable(uint8_t index) const {
    uintptr_t metadata = nn_public::VPU_METADATA_STORAGE_ADDR;

    for (int j = 0; j < 1; ++j) {
        for (size_t i = 0; i < SPECIAL_SYMTAB_SIZE; ++i) {
            symTab_[j][i].st_info = static_cast<unsigned char>(elf64STInfo(elf::STB_GLOBAL, elf::STT_OBJECT));
            symTab_[j][i].st_other = STV_DEFAULT;
            symTab_[j][i].st_shndx = 0;
            symTab_[j][i].st_name = 0;
        }

        symTab_[j][VPU_NNRD_SYM_NNCXM_SLICE_BASE_ADDR].st_value = nn_public::VPU_WORKSPACE_ADDR;
        symTab_[j][VPU_NNRD_SYM_NNCXM_SLICE_BASE_ADDR].st_size = nn_public::VPU_METADATA_SIZE;

        metadata = nn_public::align_storage(alignof(nn_public::VpuDPUInvariant), metadata);
        symTab_[j][VPU_NNRD_SYM_RTM_IVAR].st_value = metadata;
        symTab_[j][VPU_NNRD_SYM_RTM_IVAR].st_size = nn_public::VPU_INVARIANT_COUNT;
        metadata += nn_public::VPU_INVARIANT_COUNT * sizeof(nn_public::VpuDPUInvariant);

        // Not used here but must be considered for proper offsetting of the following areas
        metadata = nn_public::align_storage(alignof(nn_public::VpuDPUVariant), metadata);
        metadata += nn_public::VPU_VARIANT_COUNT * sizeof(nn_public::VpuDPUVariant);

        metadata = nn_public::align_storage(alignof(nn_public::VpuActKernelRange), metadata);
        symTab_[j][VPU_NNRD_SYM_RTM_ACT].st_value = metadata;
        symTab_[j][VPU_NNRD_SYM_RTM_ACT].st_size = nn_public::VPU_KERNEL_RANGE_COUNT;
        metadata += nn_public::VPU_KERNEL_RANGE_COUNT * sizeof(nn_public::VpuActKernelRange);

        // Not used here but must be considered for proper offsetting of the following areas
        metadata = nn_public::align_storage(alignof(nn_public::VpuActKernelInvocation), metadata);
        metadata += nn_public::VPU_KERNEL_INVO_COUNT * sizeof(nn_public::VpuActKernelInvocation);

        // DMA tasks should be further split into lists based on DDR/CMX origin
        metadata = nn_public::align_storage(alignof(nn_public::VpuDMATask), metadata);
        symTab_[j][VPU_NNRD_SYM_RTM_DMA0].st_value = metadata;
        symTab_[j][VPU_NNRD_SYM_RTM_DMA0].st_size = nn_public::VPU_DMA_TASK_COUNT;
        metadata += nn_public::VPU_DMA_TASK_COUNT * sizeof(nn_public::VpuDMATask);

        symTab_[j][VPU_NNRD_SYM_RTM_DMA1].st_value = 0x0;
        symTab_[j][VPU_NNRD_SYM_RTM_DMA1].st_size = 0;

        symTab_[j][VPU_NNRD_SYM_FIFO_BASE].st_value = 0x0;
        symTab_[j][VPU_NNRD_SYM_FIFO_BASE].st_size = 0;

        symTab_[j][VPU_NNRD_SYM_BARRIERS_START].st_value = 0;
        symTab_[j][VPU_NNRD_SYM_BARRIERS_START].st_size = 0;

        symTab_[j][VPU_NNRD_SYM_HW_REGISTER].st_value = 0;
        symTab_[j][VPU_NNRD_SYM_HW_REGISTER].st_size = 0;
    }

    VPUX_ELF_THROW_UNLESS((index != 0 && index <= N_TABS), ArgsError, "The sym tab configuration is not supported!");

    // Return configuration of index -1, because the configuration list begins at 0
    // 0 - single tile
    // 1 - multi tile
    return ArrayRef<SymbolEntry>(symTab_[index - 1], SPECIAL_SYMTAB_SIZE);
}

DeviceBuffer HostParsedInference_4000::allocateHostParsedInference(BufferManager *bufferManager) {
    auto ptr = bufferManager->allocate(BufferSpecs(
        DEFAULT_ALIGN, utils::alignUp(sizeof(nn_public::VpuHostParsedInference), DEFAULT_ALIGN), SHF_EXECINSTR));

    auto perfMetrics =
        bufferManager->allocate(BufferSpecs(DEFAULT_ALIGN, sizeof(nn_public::VpuPerformanceMetrics), SHF_EXECINSTR));
    reinterpret_cast<nn_public::VpuHostParsedInference *>(ptr.cpu_addr())->performance_metrics_.ptr =
        reinterpret_cast<uint64_t>(perfMetrics.cpu_addr());

    return ptr;
}

void HostParsedInference_4000::setHostParsedInference(DeviceBuffer &devBuffer, uint64_t mapped_entry,
                                                      ResourceRequirements resReq) {
    auto hpi = reinterpret_cast<nn_public::VpuHostParsedInference *>(devBuffer.cpu_addr());

    hpi->resource_requirements_.nn_slice_count_ = resReq.nn_slice_count_;
    hpi->resource_requirements_.nn_barriers_ = resReq.nn_barriers_;
    setDefaultPerformanceMetrics(*hpi->performance_metrics_);

    hpi->mapped_ = *reinterpret_cast<nn_public::VpuMappedInference *>(mapped_entry);
}

} // namespace elf
