// clang-format off

#include <vpux_sym_tab/3720/SymTabGen.h>
#include "api/vpu_nnrt_api.h"
#include "api/vpu_cmx_info_mtl.h"

// clang-format on

namespace elf {

SymbolEntry SymTabGen::symTab_[SymTabGen::N_TABS][SymTabGen::SPECIAL_SYMTAB_SIZE];

SymTabGen::SymTabGen() {
    initSymTab();
}

void SymTabGen::initSymTab() {
    nn_public::VpuMetadataMapSingle* single =
            reinterpret_cast<nn_public::VpuMetadataMapSingle*>(nn_public::METADATA0_STORAGE_ADDR);
    nn_public::VpuMetadataMapDual0* dual0 =
            reinterpret_cast<nn_public::VpuMetadataMapDual0*>(nn_public::METADATA0_STORAGE_ADDR);
    nn_public::VpuMetadataMapDual1* dual1 =
            reinterpret_cast<nn_public::VpuMetadataMapDual1*>(nn_public::METADATA1_STORAGE_ADDR);

    Elf64_Addr inv_addr[] = {reinterpret_cast<Elf64_Addr>(single->inv_storage),
                           reinterpret_cast<Elf64_Addr>(dual0->inv_storage)};

    Elf64_Addr akr_addr[] = {reinterpret_cast<Elf64_Addr>(single->akr_storage),
                           reinterpret_cast<Elf64_Addr>(dual0->akr_storage)};

    Elf64_Addr dma0_addr[] = {reinterpret_cast<Elf64_Addr>(single->dma_storage),
                            reinterpret_cast<Elf64_Addr>(dual0->dma0_storage)};

    Elf64_Addr dma1_addr[] = {reinterpret_cast<Elf64_Addr>(single->dma_storage),
                            reinterpret_cast<Elf64_Addr>(dual1->dma1_storage)};

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
    }
}

ArrayRef<SymbolEntry> SymTabGen::getSymTab(uint8_t index) {
    static SymTabGen symTab;

    VPUX_ELF_THROW_UNLESS((index != 0 && index <= N_TABS), ArgsError, "The sym tab configuration is not supported!");

    // Return configuration of index -1, because the configuration list begins at 0
    // 0 - single tile
    // 1 - multi tile
    return ArrayRef<SymbolEntry>(symTab_[index - 1], SPECIAL_SYMTAB_SIZE);
}

}  // namespace elf
