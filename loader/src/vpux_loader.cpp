//
// Copyright Intel Corporation.
//
// LEGAL NOTICE: Your use of this software and any required dependent software
// (the "Software Package") is subject to the terms and conditions of
// the Intel(R) OpenVINO(TM) Distribution License for the Software Package,
// which may also include notices, disclaimers, or license terms for
// third party or open source software included in or with the Software Package,
// and your use indicates your acceptance of all such terms. Please refer
// to the "third-party-programs.txt" or other similarly-named text file
// included with the Software Package for additional details.
//

#include <vpux_loader/vpux_loader.hpp>
#include <vpux_elf/types/vpu_extensions.hpp>
#include <vpux_elf/utils/error.hpp>

#ifndef VPUX_ELF_LOG_UNIT_NAME
#define VPUX_ELF_LOG_UNIT_NAME "VpuxLoader"
#endif
#include <vpux_elf/utils/log.hpp>

namespace elf {

namespace {

const uint32_t ADDRESS_MASK = ~0x00C0'0000u;
const uint64_t SLICE_LENGTH = 2 * 1024 * 1024;

uint32_t to_dpu_multicast(uint32_t addr, unsigned int &offset1, unsigned int &offset2,
                                           unsigned int &offset3) {
    const uint32_t bare_ptr = addr & ADDRESS_MASK;
    const uint32_t broadcast_mask = (addr & ~ADDRESS_MASK) >> 20;

    static const unsigned short multicast_masks[16] = {
        0x0000, 0x0001, 0x0002, 0x0003, 0x0012, 0x0011, 0x0010, 0x0030,
        0x0211, 0x0210, 0x0310, 0x0320, 0x3210, 0x3210, 0x3210, 0x3210,
    };

    VPUX_ELF_THROW_UNLESS(broadcast_mask < 16, "Broadcast mask out of range");
    const unsigned short multicast_mask = multicast_masks[broadcast_mask];

    VPUX_ELF_THROW_UNLESS(multicast_mask != 0xffff, "Got an invalid multicast mask");

    unsigned int base_mask = (static_cast<unsigned int>(multicast_mask) & 0xf) << 20;
    offset1 *= (multicast_mask >> 4) & 0xf;
    offset2 *= (multicast_mask >> 8) & 0xf;
    offset3 *= (multicast_mask >> 12) & 0xf;

    return bare_ptr | base_mask;
}

uint32_t to_dpu_multicast_base(uint32_t addr) {
    unsigned int offset1, offset2, offset3;
    return to_dpu_multicast(addr, offset1, offset2, offset3);
}

const auto VPU_64_BIT_Relocation = [](void* targetAddr, const elf::SymbolEntry&  targetSym, const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint64_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t\t64Bit Reloc addr %p symval 0x%lx addnd %lu", addr, symVal, addend);

    *addr = symVal + addend;
};

const auto VPU_64_BIT_OR_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym, const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint64_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t\t64Bit OR reloc, addr %p addrVal 0x%lx symVal 0x%lx addend %lu",addr, *addr, symVal, addend);

    *addr |= symVal + addend;
};

const auto VPU_64_BIT_LSHIFT_Relocation = [](void* targetAddr, const elf::SymbolEntry& targetSym, const Elf_Sxword addend) -> void {
    (void)addend;//hush compiler warning;
    auto addr = reinterpret_cast<uint64_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t\t64Bit LSHIFT reloc, addr %p addrVal 0x%lx symVal 0x%lx", addr, *addr, symVal);

    *addr <<= symVal;
};

const auto VPU_DISP40_RTM_RELOCATION = [](void* targetAddr, const elf::SymbolEntry& targetSym, const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint64_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    auto symSize = targetSym.st_size;
    uint64_t mask = 0xffffffffff;
    uint64_t maskedAddr = *addr & mask;
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t\tDSIP40 reloc, addr %p symVal 0x%lx symSize %lu addend %lu", addr, symVal, symSize, addend);

    *addr |= (symVal + (addend * (maskedAddr & (symSize -1)))) & mask;
};

const auto VPU_32_BIT_Relocation = [](void* targetAddr, const elf::SymbolEntry&  targetSym, const Elf_Sxword addend) -> void {
    auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t\t32Bit reloc, addr %p symVal 0x%lx addend %lu", addr, symVal, addend);

    *addr = symVal + addend;
};

const auto VPU_32_BIT_OR_Relocation = [](void* targetAddr, const elf::SymbolEntry&  targetSym, const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t\t32Bit OR reloc, addr %p addrVal 0x%x symVal 0x%lx addend %lu", addr, *addr, symVal, addend);

    *addr |= symVal + addend;
};

const auto VPU_32_BIT_RTM_Relocation = [](void* targetAddr, const elf::SymbolEntry&  targetSym, const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    auto symSize = targetSym.st_size;
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t\t32Bit RTM reloc, addr %p addrVal 0x%x symVal 0x%lx addend %lu",addr, *addr, symVal, addend);

    *addr = symVal + (addend * (*addr & (symSize -1)));
};

const auto VPU_32_BIT_SUM_Relocation = [](void* targetAddr, const elf::SymbolEntry&  targetSym, const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t\t32Bit SUM reloc, addr %p addrVal 0x%x symVal 0x%lx addend %lu", addr, *addr, symVal, addend);

    *addr += symVal + addend;
};

const auto VPU_32_MULTICAST_BASE_Relocation = [](void* targetAddr, const elf::SymbolEntry&  targetSym, const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t\t32Bit SUM reloc, addr %p addrVal 0x%x symVal 0x%lx addend %lu", addr, *addr, symVal, addend);

    *addr = to_dpu_multicast_base(symVal + addend);
};

const auto VPU_32_MULTICAST_BASE_SUB_Relocation = [](void* targetAddr, const elf::SymbolEntry&  targetSym, const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t\t32Bit SUM reloc, addr %p addrVal 0x%x symVal 0x%lx addend %lu", addr, *addr, symVal, addend);

    *addr = to_dpu_multicast_base(symVal + addend) - *addr;
};

const auto VPU_DISP28_MULTICAST_OFFSET_Relocation = [](void* targetAddr, const elf::SymbolEntry&  targetSym, const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t\t32Bit SUM reloc, addr %p addrVal 0x%x symVal 0x%lx addend %lu", addr, *addr, symVal, addend);

    unsigned int offs[3] = {SLICE_LENGTH >> 4, SLICE_LENGTH >> 4, SLICE_LENGTH >> 4}; // 1024 * 1024 >> 4 as HW requirement
    to_dpu_multicast(symVal + addend, offs[0], offs[1], offs[2]);

    const auto index = *addr >> 4;
    *addr &= 0xf;
    *addr |= offs[index] << 4;
};

const auto VPU_DISP4_MULTICAST_OFFSET_Relocation = [](void* targetAddr, const elf::SymbolEntry&  targetSym, const Elf_Sxword addend) -> void {
    const auto addr = reinterpret_cast<uint32_t*>(targetAddr);
    auto symVal = targetSym.st_value;
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t\t32Bit SUM reloc, addr %p addrVal 0x%x symVal 0x%lx addend %lu", addr, *addr, symVal, addend);

    unsigned int offs[3] = {SLICE_LENGTH >> 4, SLICE_LENGTH >> 4, SLICE_LENGTH >> 4}; // 1024 * 1024 >> 4 as HW requirement
    to_dpu_multicast(symVal + addend, offs[0], offs[1], offs[2]);

    const auto index = *addr & 0xf;
    *addr &= 0xfffffff0;
    *addr |= offs[index] != 0;
};

}

const std::map<Elf_Word,VPUXLoader::Action> VPUXLoader::actionMap = {
    {SHT_NULL       , Action::None},
    {SHT_PROGBITS   , Action::AllocateAndLoad},
    {SHT_SYMTAB     , Action::RegisterUserIO},
    {SHT_STRTAB     , Action::None},
    {SHT_RELA       , Action::Relocate},
    {SHT_HASH       , Action::Error},
    {SHT_DYNAMIC    , Action::Error},
    {SHT_NOTE       , Action::Error},
    {SHT_NOBITS     , Action::Allocate},
    {SHT_REL        , Action::Error},
    {SHT_SHLIB      , Action::Error},
    {SHT_DYNSYM     , Action::Error},
    {VPU_SHT_NETDESC, Action::RegisterNetworkMetadata},
};

const std::map<VPUXLoader::RelocationType, VPUXLoader::RelocationFunc> VPUXLoader::relocationMap = {
    {R_VPU_64                           , VPU_64_BIT_Relocation},
    {R_VPU_64_OR                        , VPU_64_BIT_OR_Relocation},
    {R_VPU_DISP40_RTM                   , VPU_DISP40_RTM_RELOCATION},
    {R_VPU_64_LSHIFT                    , VPU_64_BIT_LSHIFT_Relocation},
    {R_VPU_32                           , VPU_32_BIT_Relocation},
    {R_VPU_32_RTM                       , VPU_32_BIT_RTM_Relocation},
    {R_VPU_32_SUM                       , VPU_32_BIT_SUM_Relocation},
    {R_VPU_32_MULTICAST_BASE            , VPU_32_MULTICAST_BASE_Relocation},
    {R_VPU_32_MULTICAST_BASE_SUB        , VPU_32_MULTICAST_BASE_SUB_Relocation},
    {R_VPU_DISP28_MULTICAST_OFFSET      , VPU_DISP28_MULTICAST_OFFSET_Relocation},
    {R_VPU_DISP4_MULTICAST_OFFSET_CMP   , VPU_DISP4_MULTICAST_OFFSET_Relocation},
};

VPUXLoader::VPUXLoader(void* elf, size_t elfSize, details::ArrayRef<SymbolEntry> runtimeSymTabs, BufferManager* bufferManager) :
                        m_reader(reinterpret_cast<const uint8_t*>(elf), elfSize), m_runtimeSymTabs(runtimeSymTabs),
                        m_bufferManager(bufferManager), m_allocatedZones(), m_sectionToAddr(), m_jitRelocations(),
                        m_userInputs(), m_userOutputs() {
    load();
};

VPUXLoader::~VPUXLoader() {
    clean();
}

uint64_t VPUXLoader::getEntry() {

    //this is very very temporary version
    auto numSections = m_reader.getSectionsNum();

    for (size_t sectionCtr = 0; sectionCtr < numSections; ++sectionCtr) {

        auto section = m_reader.getSection(sectionCtr);

        auto hdr = section.getHeader();
        if(hdr->sh_type == elf::SHT_SYMTAB) {
            auto symTabsSize = section.getEntriesNum();
            auto symTabs = section.getData<elf::SymbolEntry>();

            for(size_t symTabIdx = 0; symTabIdx < symTabsSize; ++symTabIdx) {
                auto& symTab = symTabs[symTabIdx];
                auto symType = elf64STType(symTab.st_info);
                if(symType == VPU_STT_ENTRY) {
                    auto secIndx = symTab.st_shndx;
                    return m_sectionToAddr[secIndx].vpu_addr();
                }
            }
        }
    }

    return 0;
}

void VPUXLoader::load() {
    VPUX_ELF_LOG(LogLevel::TRACE, "Starting LOAD process");
    auto numSections = m_reader.getSectionsNum();

    m_sectionToAddr.resize(numSections);
    m_allocatedZones.reserve(numSections);

    std::vector<int> relocationSectionIndexes;
    relocationSectionIndexes.reserve(numSections);
    m_jitRelocations.reserve(2);

    VPUX_ELF_LOG(LogLevel::DEBUG,"Got elf wiht %lu sections", numSections);
    for (size_t sectionCtr = 0; sectionCtr < numSections; ++sectionCtr) {
        VPUX_ELF_LOG(LogLevel::DEBUG,"Solving section %lu", sectionCtr);

        const auto& section = m_reader.getSection(sectionCtr);

        const auto sectionHeader = section.getHeader();
        auto sectionType = sectionHeader->sh_type;
        auto searchAction = actionMap.find(sectionType);

        VPUX_ELF_THROW_WHEN(searchAction == actionMap.end(), "Unknown section type");

        auto sectionFlags = sectionHeader->sh_flags;
        auto action = searchAction->second;

        VPUX_ELF_LOG(LogLevel::DEBUG,"    name  : %s",section.getName());
        VPUX_ELF_LOG(LogLevel::DEBUG,"    type  : %u",sectionType);
        VPUX_ELF_LOG(LogLevel::DEBUG,"    flags : 0x%lx",sectionFlags);
        VPUX_ELF_LOG(LogLevel::DEBUG,"    action: %u",(uint32_t)action);

        switch(action) {

        case Action::AllocateAndLoad: {
            VPUX_ELF_LOG(LogLevel::TRACE,"Allocate and loading %lu", sectionCtr);

            auto sectionSize = sectionHeader->sh_size;
            auto sectionAlignment = sectionHeader->sh_addralign;

            DeviceBuffer devBuf = m_bufferManager->allocate(BufferSpecs(sectionAlignment, sectionSize, sectionFlags));

            VPUX_ELF_THROW_WHEN(devBuf.cpu_addr() == nullptr || devBuf.size() < sectionSize, "Failed to allocate for section");

            m_bufferManager->lock(devBuf);
            m_bufferManager->copy(devBuf, section.getData<uint8_t>(), sectionSize);
            m_bufferManager->unlock(devBuf);

            m_allocatedZones.push_back(devBuf);
            m_sectionToAddr[sectionCtr] = devBuf;

            VPUX_ELF_LOG(LogLevel::DEBUG,"\tFor section %s Allocated %p of size  %lu and copied from %p to %p", section.getName(), devBuf.cpu_addr(), sectionSize, section.getData<uint8_t>() , section.getData<uint8_t>() + sectionSize);
            break;
        }

        case Action::Allocate: {
            VPUX_ELF_LOG(LogLevel::TRACE,"Allocating %lu", sectionCtr);

            auto sectionSize = sectionHeader->sh_size;
            auto sectionAlignment = sectionHeader->sh_addralign;

            DeviceBuffer devBuf = m_bufferManager->allocate(BufferSpecs(sectionAlignment, sectionSize, sectionFlags));
            VPUX_ELF_THROW_WHEN(devBuf.cpu_addr() == nullptr || devBuf.size() < sectionSize, "Failed to allocate for section");

            m_allocatedZones.push_back(devBuf);
            m_sectionToAddr[sectionCtr] = devBuf;

            VPUX_ELF_LOG(LogLevel::DEBUG,"\tFor section %s Allocated %p of size %lu", section.getName(), devBuf.cpu_addr(), sectionSize);
            break;
        }

        case Action::Relocate: {
            if(sectionFlags & VPU_SHF_JIT) {
                VPUX_ELF_LOG(LogLevel::DEBUG,"Registering JIT Relocation %lu", sectionCtr);
                m_jitRelocations.push_back(sectionCtr);
            }
            else {
                relocationSectionIndexes.push_back(sectionCtr);
                VPUX_ELF_LOG(LogLevel::DEBUG,"Registering Relocation %lu", sectionCtr);
            }
            break;
        }

        case Action::RegisterUserIO: {
            VPUX_ELF_LOG(LogLevel::DEBUG,"Parsed symtab section with flags %lx", sectionFlags);

            if(sectionFlags & VPU_SHF_USERINPUT) {
                VPUX_ELF_THROW_WHEN(m_userInputs.size(),"User inputs already read.... potential more than one input section?");

                VPUX_ELF_LOG(LogLevel::DEBUG,"\tRegistering %lu inputs", section.getEntriesNum() -1);
                registerUserIO(m_userInputs, section.getData<elf::SymbolEntry>(), section.getEntriesNum());
            }
            else if(sectionFlags & VPU_SHF_USEROUTPUT) {
                VPUX_ELF_THROW_WHEN(m_userOutputs.size(),"User outputs already read.... potential more than one output section?");

                VPUX_ELF_LOG(LogLevel::DEBUG,"\tRegistering %lu outputs", section.getEntriesNum() -1);
                registerUserIO(m_userOutputs, section.getData<elf::SymbolEntry>(), section.getEntriesNum());
            }
            break;
        }

            case Action::RegisterNetworkMetadata: {
                VPUX_ELF_LOG(LogLevel::DEBUG, "Resource Requirements:");

                // only getting the top of the section which contains a structure with resource requirements
                m_resourceRequirements = *(section.getData<elf::ResourceRequirements>());


                // the number of available barriers is computed as follows:
                // numClusters - (to be used) platform specific
                // maxNumClustersForArch - platform specific
                // maxBarriersPerInference - platrofm specific
                // barriersPerCluster = maxBarriersPerInference / maxNumClustersForArch
                // nn_barriers = min(maxBarriersPerInference, barriersPerCluster * numClusters)
                VPUX_ELF_LOG(LogLevel::DEBUG, "\t\tnn_barriers %hhu", m_resourceRequirements.nn_barriers_);
                VPUX_ELF_LOG(LogLevel::DEBUG, "\t\tnn_slice_count_ %hhu", m_resourceRequirements.nn_slice_count_);

                // not uesd:
                VPUX_ELF_LOG(LogLevel::DEBUG, "\t\tnn_barrier_count_ %hu", m_resourceRequirements.nn_barrier_count_);
                VPUX_ELF_LOG(LogLevel::DEBUG, "\t\tnn_slice_length_ %u", m_resourceRequirements.nn_slice_length_);
                VPUX_ELF_LOG(LogLevel::DEBUG, "\t\tddr_scratch_length_ %u", m_resourceRequirements.ddr_scratch_length_);
                break;
            }

        case Action::Error: {
            VPUX_ELF_THROW("Unexpected section type");
            return;
        }

        case Action::None: {
            break;
        }

        default: {
            VPUX_ELF_THROW("Unhandled section type");
            return;
        }
        }
    }

    applyRelocations(relocationSectionIndexes);

    VPUX_ELF_LOG(LogLevel::INFO,"Allocated %lu sections", m_allocatedZones.size());
    VPUX_ELF_LOG(LogLevel::INFO,"Registered %lu inputs of sizes: ", m_userInputs.size());
    for(size_t inputCtr = 0; inputCtr < m_userInputs.size(); ++inputCtr) {
        VPUX_ELF_LOG(LogLevel::INFO,"\t %lu : %lu", inputCtr, m_userInputs[inputCtr].size());
    }
    VPUX_ELF_LOG(LogLevel::INFO,"Registered %lu outputs of sizes: ", m_userOutputs.size());
    for(size_t outputCtr = 0; outputCtr < m_userOutputs.size(); ++outputCtr) {
        VPUX_ELF_LOG(LogLevel::INFO,"\t %lu : %lu", outputCtr, m_userOutputs[outputCtr].size());
    }

    return;
};

void VPUXLoader::applyRelocations(details::ArrayRef<int> relocationSectionIndexes) {

    VPUX_ELF_LOG(LogLevel::TRACE,"apply relocations");
    for(const auto& relocationSectionIdx : relocationSectionIndexes) {

        VPUX_ELF_LOG(LogLevel::DEBUG,"applying relocation section %u", relocationSectionIdx);

        const auto& relocSection = m_reader.getSection(relocationSectionIdx);
        auto relocations = relocSection.getData<elf::RelocationAEntry>();
        auto relocSecHdr = relocSection.getHeader();
        auto numRelocs = relocSection.getEntriesNum();

        VPUX_ELF_LOG(LogLevel::DEBUG,"\tRelA section with %lu elements at addr %p", numRelocs, relocations);
        VPUX_ELF_LOG(LogLevel::DEBUG,"\tRelA section info, link flags 0x%x %u 0x%lx", relocSecHdr->sh_info,
                    relocSecHdr->sh_link, relocSecHdr->sh_flags);

        //find the links to this relocation section
        auto symTabIdx = relocSecHdr->sh_link;

        //by convention, we will assume symTabIdx==VPU_RT_SYMTAB to be the "built-in" symtab
        auto getSymTab = [&]() -> const SymbolEntry* {
            if(symTabIdx == VPU_RT_SYMTAB) {
                return m_runtimeSymTabs.data();
            }

            auto symTabSection = m_reader.getSection(symTabIdx);
            auto symTabSectionHdr = symTabSection.getHeader();

            VPUX_ELF_THROW_UNLESS(checkSectionType(symTabSectionHdr, elf::SHT_SYMTAB),
                "Reloc section pointing to snon-symtab");

            return symTabSection.getData<elf::SymbolEntry>();

        };

        auto symTabs = getSymTab();

        auto relocSecFlags = relocSecHdr->sh_flags;
        Elf_Word targetSectionIdx = 0;
        if(relocSecFlags & SHF_INFO_LINK) {
            targetSectionIdx = relocSecHdr->sh_info;
        }
        else {
            VPUX_ELF_THROW("Rela section with no target section");    // TODO(E#30067): Review if there is a case where we should accept rela sections w/o a target section?
                                                            // This is generally used for executable files, but we would only generate relocatable files
            return;
        }

        VPUX_ELF_THROW_WHEN(targetSectionIdx == 0 || targetSectionIdx > m_reader.getSectionsNum(), "invalid target section from rela section");

        //at this point we assume that all sections have an address, to which we can apply a simple lookup
        auto targetSectionDevBuf = m_sectionToAddr[targetSectionIdx];
        m_bufferManager->lock(targetSectionDevBuf);
        auto targetSectionAddr = targetSectionDevBuf.cpu_addr();

        VPUX_ELF_LOG(LogLevel::DEBUG,"\tTargetsectionAddr %p", targetSectionAddr);

        //apply the actual relocations
        for(size_t relocIdx = 0; relocIdx < numRelocs; ++relocIdx) {
            const elf::RelocationAEntry& relocation = relocations[relocIdx];

            auto relOffset = relocation.r_offset;
            auto relSymIdx = elf64RSym(relocation.r_info);
            auto relType = elf64RType(relocation.r_info);
            auto addend = relocation.r_addend;

            auto reloc = relocationMap.find( static_cast<RelocationType>(relType));
            VPUX_ELF_THROW_WHEN(reloc == relocationMap.end() || reloc->second == nullptr, "Invalid relocation type detected");

            auto relocFunc = reloc->second;

            //the actual data that we need to modify
            auto relocationTargetAddr = targetSectionAddr + relOffset;

            //deliberate copy so we don't modify the contents of the original elf.
            elf::SymbolEntry targetSymbol = symTabs[relSymIdx];
            auto symbolTargetSectionIdx = targetSymbol.st_shndx;
            targetSymbol.st_value += (elf::Elf64_Addr)m_sectionToAddr[symbolTargetSectionIdx].vpu_addr();

            VPUX_ELF_LOG(LogLevel::DEBUG, "\t\tApplying Relocation at offset %lu symidx %u reltype %u addend %lu", relOffset, relSymIdx, relType, addend);

            relocFunc((void*)relocationTargetAddr,targetSymbol, addend);
        }

        m_bufferManager->unlock(targetSectionDevBuf);
    }

    return;
};

//TODO(E#30069) : a lot of shared logic with applyRelocations.... refactor to share code.... duplicate for WIP purposes
void VPUXLoader::applyJitRelocations(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs) {

    VPUX_ELF_LOG(LogLevel::TRACE,"apply JITrelocations");
    for(const auto& relocationSectionIdx : m_jitRelocations) {

        VPUX_ELF_LOG(LogLevel::DEBUG,"\tapplying JITrelocation section %u", relocationSectionIdx);

        const auto& relocSection = m_reader.getSection(relocationSectionIdx);
        auto relocations = relocSection.getData<elf::RelocationAEntry>();
        auto relocSecHdr = relocSection.getHeader();
        auto numRelocs = relocSection.getEntriesNum();

        VPUX_ELF_LOG(LogLevel::DEBUG,"\tJitRelA section with %lu elements at addr %p", numRelocs, relocations);
        VPUX_ELF_LOG(LogLevel::DEBUG,"\tJitRelA section info, link flags 0x%x %u 0x%lx", relocSecHdr->sh_info, relocSecHdr->sh_link, relocSecHdr->sh_flags);

        auto symTabIdx = relocSecHdr->sh_link;

        //in JitRelocations case, we will expect to point to either "VPUX_USER_INPUT" or "VPUX_USER_INPUT" symtabs
        VPUX_ELF_THROW_WHEN(symTabIdx == VPU_RT_SYMTAB, "JitReloc pointing to runtime symtab idx");

        auto symTabSection = m_reader.getSection(symTabIdx);
        auto symTabSectionHdr = symTabSection.getHeader();

        VPUX_ELF_THROW_UNLESS(checkSectionType(symTabSectionHdr, elf::SHT_SYMTAB), "Reloc section pointing to non-symtab");

        auto symTabSize = symTabSection.getEntriesNum();
        auto symTabs = symTabSection.getData<elf::SymbolEntry>();

        VPUX_ELF_LOG(LogLevel::DEBUG,"\tSymTabIdx %u symTabSize %lu at %p",symTabIdx,symTabSize,symTabs);

        auto relocSecFlags = relocSecHdr->sh_flags;
        auto getUserAddrs = [&]() -> details::ArrayRef<DeviceBuffer> {
            if(relocSecFlags & VPU_SHF_USERINPUT) {
                return details::ArrayRef<DeviceBuffer>(inputs);
            }
            else if(relocSecFlags & VPU_SHF_USEROUTPUT) {
                return details::ArrayRef<DeviceBuffer>(outputs);
            }
            else {
                VPUX_ELF_THROW("Jit reloc section pointing neither to userInput nor userOutput");
                return details::ArrayRef<DeviceBuffer>(outputs);
            }
        };

        auto userAddrs = getUserAddrs();

        Elf_Word targetSectionIdx = 0;
        if(relocSecFlags & SHF_INFO_LINK) {
            targetSectionIdx = relocSecHdr->sh_info;
        }
        else {
            VPUX_ELF_THROW("Rela section with no target section");    // TODO(E#30067) : Review if there is a case where we should accept rela sections w/o a target section?
                                                            // This is generally used for executable files, but we would only generate relocatable files
            return;
        }

        //at this point we assume that all sections have an address, to which we can apply a simple lookup
        auto targetSectionDevBuf = m_sectionToAddr[targetSectionIdx];
        m_bufferManager->lock(targetSectionDevBuf);
        auto targetSectionAddr = targetSectionDevBuf.cpu_addr();

        VPUX_ELF_LOG(LogLevel::DEBUG,"\t targetSectionAddr %p", targetSectionAddr);

        //apply the actual relocations
        for(size_t relocIdx = 0; relocIdx < numRelocs; ++relocIdx) {
            VPUX_ELF_LOG(LogLevel::DEBUG,"\t Solving Reloc at %p %lu",relocations,relocIdx);

            const elf::RelocationAEntry& relocation = relocations[relocIdx];

            auto offset = relocation.r_offset;
            auto symIdx = elf64RSym(relocation.r_info);
            auto relType = elf64RType(relocation.r_info);
            auto addend = relocation.r_addend;

            VPUX_ELF_LOG(LogLevel::DEBUG,"\t\t applying Reloc offset symidx reltype addend %lu %u %u %lu",offset,symIdx,relType,addend);
            auto reloc = relocationMap.find(static_cast<RelocationType>(relType));
            VPUX_ELF_THROW_WHEN(reloc == relocationMap.end() || reloc->second == nullptr, "Invalid relocation type detected");

            auto relocFunc = reloc->second;
            auto targetAddr = targetSectionAddr + offset;

            VPUX_ELF_LOG(LogLevel::DEBUG,"\t targetsectionAddr %p offs %lu result %p userAddr 0x%x symIdx %u",
                                    targetSectionAddr,offset,targetAddr,(uint32_t)userAddrs[symIdx-1].vpu_addr(),symIdx-1);

            elf::SymbolEntry origSymbol = symTabs[symIdx];

            elf::SymbolEntry targetSymbol;
            targetSymbol.st_info = 0;
            targetSymbol.st_name = 0;
            targetSymbol.st_other = 0;
            targetSymbol.st_shndx = 0;
            targetSymbol.st_value = userAddrs[symIdx-1].vpu_addr();
            targetSymbol.st_size = origSymbol.st_size;

            relocFunc((void*)targetAddr,targetSymbol, addend);
        }

        m_bufferManager->unlock(targetSectionDevBuf);
    }
}

details::ArrayRef<DeviceBuffer> VPUXLoader::getAllocatedBuffers() {
    return details::ArrayRef<DeviceBuffer>(m_allocatedZones);
}

void VPUXLoader::registerUserIO(details::FixedVector<DeviceBuffer>& userIO,const elf::SymbolEntry* symbols, size_t symbolCount) const {

    if(symbolCount <= 1) {
        VPUX_ELF_LOG(LogLevel::WARN,"Have a USER_IO symbols section with no symbols");
        return;
    }

    userIO.resize(symbolCount -1);

    //symbol sections always start with an UNDEFINED symbol by standard
    for(size_t symbolCtr = 1; symbolCtr < symbolCount; ++symbolCtr) {
        const elf::SymbolEntry& sym = symbols[symbolCtr];
        userIO[symbolCtr-1] = DeviceBuffer(nullptr, 0, sym.st_size);
    }
}

details::ArrayRef<DeviceBuffer> VPUXLoader::getInputBuffers() {
    return details::ArrayRef<DeviceBuffer>(m_userInputs.data(), m_userInputs.size());
};

details::ArrayRef<DeviceBuffer> VPUXLoader::getOutputBuffers() {
    return details::ArrayRef<DeviceBuffer>(m_userOutputs.data(), m_userOutputs.size());
};


bool VPUXLoader::checkSectionType(const elf::SectionHeader* section, Elf_Word secType) const {
    return section->sh_type == secType;
}

void VPUXLoader::clean() {
    for(DeviceBuffer& devBuffer : m_allocatedZones) {
        m_bufferManager->deallocate(devBuffer);
    }
}

const elf::ResourceRequirements VPUXLoader::getResourceRequirements() const
{
    return this->m_resourceRequirements;
}

}
