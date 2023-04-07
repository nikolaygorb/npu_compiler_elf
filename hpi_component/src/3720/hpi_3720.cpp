
#include <vpux_loader/vpux_loader.hpp>
#include <vpux_elf/utils/utils.hpp>
#include <hpi_3720.hpp>
#include <api/vpu_nnrt_api.h>
#include "api/vpu_cmx_info_37xx.h"


namespace elf {

static constexpr uint8_t N_TABS = nn_public::VPU_MAX_TILES;
static constexpr size_t SPECIAL_SYMTAB_SIZE = 8;
static SymbolEntry symTab_[N_TABS][SPECIAL_SYMTAB_SIZE];

std::string HostParsedInference_3720::getType() const {
    return "VPUX37XX";
}

ArrayRef<SymbolEntry> HostParsedInference_3720::getSymTab(uint8_t index) const {
    uint32_t inv_addr[] = {nn_public::METADATA0_STORAGE_ADDR + offsetof(nn_public::VpuMetadataMapSingle, inv_storage),
                           nn_public::METADATA0_STORAGE_ADDR + offsetof(nn_public::VpuMetadataMapDual0, inv_storage)};

    uint32_t akr_addr[] = {nn_public::METADATA0_STORAGE_ADDR + offsetof(nn_public::VpuMetadataMapSingle, akr_storage),
                           nn_public::METADATA0_STORAGE_ADDR + offsetof(nn_public::VpuMetadataMapDual0, akr_storage)};

    uint32_t dma0_addr[] = {nn_public::METADATA0_STORAGE_ADDR + offsetof(nn_public::VpuMetadataMapSingle, dma_storage),
                            nn_public::METADATA0_STORAGE_ADDR + offsetof(nn_public::VpuMetadataMapDual0, dma0_storage)};

    uint32_t dma1_addr[] = {nn_public::METADATA0_STORAGE_ADDR + offsetof(nn_public::VpuMetadataMapSingle, dma_storage),
                            nn_public::METADATA1_STORAGE_ADDR + offsetof(nn_public::VpuMetadataMapDual1, dma1_storage)};

    for (int j = 0; j < N_TABS; ++j) {
        for (size_t i = 0; i < SPECIAL_SYMTAB_SIZE; ++i) {
            symTab_[j][i].st_info = static_cast<unsigned char>(elf64STInfo(elf::STB_GLOBAL, elf::STT_OBJECT));
            symTab_[j][i].st_other = STV_DEFAULT;
            symTab_[j][i].st_shndx = 0;
            symTab_[j][i].st_name = 0;
        }

        symTab_[j][VPU_NNRD_SYM_NNCXM_SLICE_BASE_ADDR].st_value = nn_public::VPU_WORKSPACE_ADDR_0;
        symTab_[j][VPU_NNRD_SYM_NNCXM_SLICE_BASE_ADDR].st_size = nn_public::VPU_WORKSPACE_SIZE;

        symTab_[j][VPU_NNRD_SYM_RTM_IVAR].st_value = inv_addr[j];
        symTab_[j][VPU_NNRD_SYM_RTM_IVAR].st_size = nn_public::VPU_INVARIANT_COUNT;

        symTab_[j][VPU_NNRD_SYM_RTM_ACT].st_value = akr_addr[j];
        symTab_[j][VPU_NNRD_SYM_RTM_ACT].st_size = nn_public::VPU_KERNEL_RANGE_COUNT;

        symTab_[j][VPU_NNRD_SYM_RTM_DMA0].st_value = dma0_addr[j];
        symTab_[j][VPU_NNRD_SYM_RTM_DMA0].st_size = nn_public::VPU_DMA_TASK_COUNT;

        symTab_[j][VPU_NNRD_SYM_RTM_DMA1].st_value = dma1_addr[j];
        symTab_[j][VPU_NNRD_SYM_RTM_DMA1].st_size = nn_public::VPU_DMA_TASK_COUNT;

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

DeviceBuffer HostParsedInference_3720::allocHostParsedInference(BufferManager *bufferManager) {
    // to be dealocated by the caller of this method.
    // this will only return an architecture specific
    // pointer to a strucutre
    return bufferManager->allocate(
        BufferSpecs(DEFAULT_ALIGN, utils::alignUp(sizeof(nn_public::VpuHostParsedInference), DEFAULT_ALIGN), SHF_EXECINSTR));
}

void HostParsedInference_3720::setHostParsedInference(DeviceBuffer &devBuffer, uint64_t mapped_entry, ResourceRequirements resReq) {
    auto hpi = reinterpret_cast<nn_public::VpuHostParsedInference *>(devBuffer.cpu_addr());

    hpi->resource_requirements_ = {};
    hpi->resource_requirements_.nn_slice_count_ = resReq.nn_slice_count_;
    hpi->resource_requirements_.nn_barriers_ = resReq.nn_barriers_;
    hpi->performance_metrics_ = {};

    hpi->mapped_.address = mapped_entry;
    hpi->mapped_.count = 1;
}

} // namespace elf
