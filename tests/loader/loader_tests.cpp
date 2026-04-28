//
// Copyright (C) 2025-2026 Intel Corporation.
// SPDX-License-Identifier: Apache-2.0
//

#include <gtest/gtest.h>

#include <malloc.h>
#include <stdint.h>
#include <cstdint>
#include <cstring>
#include <exception>
#include <random>
#include <vector>

#include "allocator_utils/buffer_managers.hpp"
#include "test_blob/binary_actions.hpp"
#include "test_blob/interfaces.hpp"
#include "test_blob/relocation_actions.hpp"
#include "test_blob/symbol_actions.hpp"
#include "test_blob/test_blob.hpp"

#include "vpux_elf/accessor.hpp"
#include "vpux_elf/types/dma_symbol_entry.hpp"
#include "vpux_elf/types/section_header.hpp"
#include "vpux_elf/types/symbol_entry.hpp"
#include "vpux_elf/types/vpu_extensions.hpp"
#include "vpux_headers/metadata.hpp"
#include "vpux_headers/serial_metadata.hpp"
#include "vpux_loader/vpux_loader.hpp"

#include <vpux_headers/dma_hw_npu4.hpp>

using namespace elf;
using namespace writer;

// TODO: move to utils
#define OV_ASSERT_NO_THROW(statement) OV_ASSERT_NO_THROW_(statement, GTEST_FATAL_FAILURE_)
#define OV_ASSERT_NO_THROW_(statement, fail)                              \
    GTEST_AMBIGUOUS_ELSE_BLOCKER_                                         \
    if (::testing::internal::AlwaysTrue()) {                              \
        try {                                                             \
            GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);    \
        } catch (const std::exception& e) {                               \
            fail("Expected: " #statement " doesn't throw an exception.\n" \
                 "  Actual: it throws.")                                  \
                    << e.what();                                          \
        } catch (...) {                                                   \
            fail("Expected: " #statement " doesn't throw an exception.\n" \
                 "  Actual: it throws.");                                 \
        }                                                                 \
    }

uint32_t generateRandom(uint32_t from, uint32_t to) {
    std::mt19937 gen((std::random_device())());
    std::uniform_int_distribution<> dist(from, to);

    return dist(gen);
}

namespace {

class HardCodedSymtabToCluster0 {
private:
    static constexpr size_t SPECIAL_SYMTAB_SIZE = 7;
    SymbolEntry symTab_[SPECIAL_SYMTAB_SIZE];

public:
    HardCodedSymtabToCluster0(): symTab_() {
        for (size_t i = 0; i < SPECIAL_SYMTAB_SIZE; ++i) {
            symTab_[i].st_info = static_cast<unsigned char>(elf64STInfo(STB_GLOBAL, STT_OBJECT));
            symTab_[i].st_other = STV_DEFAULT;
            symTab_[i].st_shndx = 0;
            symTab_[i].st_name = 0;
        }

        symTab_[VPU_NNRD_SYM_NNCXM_SLICE_BASE_ADDR].st_value = 0x2e014000;
        symTab_[VPU_NNRD_SYM_NNCXM_SLICE_BASE_ADDR].st_size = 2097152;

        symTab_[VPU_NNRD_SYM_RTM_IVAR].st_value = 0x2e004000;
        symTab_[VPU_NNRD_SYM_RTM_IVAR].st_size = 64;

        symTab_[VPU_NNRD_SYM_RTM_ACT].st_value = 0;
        symTab_[VPU_NNRD_SYM_RTM_ACT].st_size = 0;

        symTab_[VPU_NNRD_SYM_RTM_DMA0].st_value = 0x2e1f8000;
        symTab_[VPU_NNRD_SYM_RTM_DMA0].st_size = 64;

        symTab_[VPU_NNRD_SYM_RTM_DMA1].st_value = 0x2e1fc000;
        symTab_[VPU_NNRD_SYM_RTM_DMA1].st_size = 64;

        symTab_[VPU_NNRD_SYM_FIFO_BASE].st_value = 0x0;
        symTab_[VPU_NNRD_SYM_FIFO_BASE].st_size = 0;

        symTab_[VPU_NNRD_SYM_BARRIERS_START].st_value = 0;
        symTab_[VPU_NNRD_SYM_BARRIERS_START].st_size = 0;
    }

    const std::vector<SymbolEntry> symTab() const {
        return std::vector<SymbolEntry>(symTab_, symTab_ + SPECIAL_SYMTAB_SIZE);
    }
};

const HardCodedSymtabToCluster0 gSymTab;

static auto validElfDefault = ActionsSequence{{

        AddDummyBinarySection::build(
                ".binSection_0",
                AddDummyBinarySection::Attributes{{}, {elf::SHT_PROGBITS, std::vector<DummyBinObject>(24)}})

}};

using AddDMADescriptorBinarySection = AddBinarySectionAction<dma_npu4::DmaDescriptor>;

static const auto validElfWithDmaRelocations = ActionsSequence{{

        AddDMADescriptorBinarySection::build(
                ".binSection_0",
                AddDMADescriptorBinarySection::Attributes{
                        {}, {elf::SHT_PROGBITS, AddDMADescriptorBinarySection::Vector(4)}}),

        AddDMASymbolSection::build(
                ".symtab_0", AddDMASymbolSection::Attributes{VPU_SHF_USERINPUT | VPU_SHF_JIT},
                ActionsSequence{{

                        AddDMASymbol::build("symtab_0_sym0",
                                            AddDMASymbol::Attributes{DmaSymbolEntry{0,
                                                                                    0,
                                                                                    {6, 2, 4, 0, 0, 0},
                                                                                    {1, 6, 12, 48, 0, 0},
                                                                                    {0, 0, 0, 0, 0, 0},
                                                                                    {6, 2, 4, 0, 0, 0},
                                                                                    {1, 6, 12, 48, 0, 0},
                                                                                    4}},
                                            AddDMASymbol::Operands{}),
                }}),

        AddDMARelocationSection::build(
                ".relocSection_0", AddDMARelocationSection::Attributes{VPU_SHF_USERINPUT | VPU_SHF_JIT},
                AddDMARelocationSection::Operands{".symtab_0", ".binSection_0"},
                ActionsSequence{

                        {AddDMARelocation::build("reloc0", AddDMARelocation::Attributes{R_VPU_DMA_TASK_INPUT, 0, 0},
                                                 AddDMARelocation::Operands{"symtab_0_sym0"}),

                         AddDMARelocation::build("reloc1", AddDMARelocation::Attributes{R_VPU_DMA_TASK_OUTPUT, 0, 0},
                                                 AddDMARelocation::Operands{"symtab_0_sym0"})

                        }}),

}};

TEST(ELFLoader, ThrowWhenAccessorPointerIsNull) {
    DummyBufferManager bufMgr;

    ASSERT_THROW(VPUXLoader(nullptr, &bufMgr), ArgsError);
}

TEST(ELFLoader, ThrowWhenElfHeaderIsInvalid) {
    ELFHeader elf{};
    elf.e_ident[0] = elf::ELFMAG0;
    elf.e_ident[1] = elf::EI_MAG1;
    elf.e_ident[2] = elf::EI_MAG2;
    elf.e_ident[3] = 'X';
    DummyBufferManager bufMgr;

    DDRAccessManager<elf::DDRAlwaysEmplace> accessor(reinterpret_cast<const uint8_t*>(&elf), sizeof(ELFHeader));

    ASSERT_THROW(VPUXLoader(&accessor, &bufMgr), HeaderError);
}

TEST(ELFLoader, ThrowWhenBufferManagerIsNull) {
    std::vector<uint8_t> elf;

    OV_ASSERT_NO_THROW(elf = TestBlob(validElfDefault).getBinary(););

    DDRAccessManager<elf::DDRAlwaysEmplace> accessor(reinterpret_cast<const uint8_t*>(elf.data()), elf.size());
    ASSERT_THROW(VPUXLoader(&accessor, nullptr), ArgsError);
}

TEST(ELFLoader, ThrowWhenAllocFails) {
    NullAllocBufferManager bufMgr;
    std::vector<uint8_t> elf;

    ASSERT_THROW(AllocatedDeviceBuffer(nullptr, {0, 0, 0}), ArgsError);
}

TEST(ELFLoader, ThrowWhenBadUserIO) {
    DummyBufferManager bufMgr;
    std::vector<uint8_t> elf;

    auto blobConfig = ActionsSequence{{

            AddDummyBinarySection::build(
                    ".binSection_0",
                    AddDummyBinarySection::Attributes{{}, {elf::SHT_PROGBITS, std::vector<DummyBinObject>(24)}}),

            AddSymbolSection::build(".symtab_0", AddSymbolSection::Attributes{VPU_SHF_USERINPUT},
                                    ActionsSequence{{

                                            AddSymbol::build("symtab_0_sym0", AddSymbol::Attributes{STT_SECTION},
                                                             AddSymbol::Operands{".binSection_0"}),

                                            AddSymbol::build("symtab_0_sym1", AddSymbol::Attributes{STT_SECTION},
                                                             AddSymbol::Operands{".binSection_0"}),

                                            AddSymbol::build("symtab_0_sym2", AddSymbol::Attributes{STT_SECTION},
                                                             AddSymbol::Operands{".binSection_0"})

                                    }}),

            AddSymbolSection::build(".symtab_1", AddSymbolSection::Attributes{VPU_SHF_USERINPUT}, ActionsSequence{}),

            AddSymbolSection::build(".symtab_2", AddSymbolSection::Attributes{VPU_SHF_USERINPUT}, ActionsSequence{}),

            AddRelocationSection::build(
                    ".relocSection_0", AddRelocationSection::Attributes{},
                    AddRelocationSection::Operands{".symtab_0", ".binSection_0"},
                    ActionsSequence{{

                            AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_64, 0, 0},
                                                 AddRelocation::Operands{"symtab_0_sym0"}),
                            AddRelocation::build("reloc1", AddRelocation::Attributes{R_VPU_64, 0, 0},
                                                 AddRelocation::Operands{"symtab_0_sym1"})

                    }}),
    }};

    OV_ASSERT_NO_THROW(elf = TestBlob(blobConfig).getBinary());

    DDRAccessManager<elf::DDRAlwaysEmplace> accessor(reinterpret_cast<const uint8_t*>(elf.data()), elf.size());
    ASSERT_THROW(VPUXLoader loader(&accessor, &bufMgr), SequenceError);
}

TEST(ELFLoader, ThrowWhenBadSectionType) {
    DummyBufferManager bufMgr;
    std::vector<uint8_t> elf;

    auto blobConfig = ActionsSequence{{AddDummyBinarySection::build(
            ".binSection_0",
            AddDummyBinarySection::Attributes{{}, {SHT_LOUSER - 2, std::vector<DummyBinObject>(24)}})}};

    OV_ASSERT_NO_THROW(elf = TestBlob(blobConfig).getBinary());

    DDRAccessManager<elf::DDRAlwaysEmplace> accessor(reinterpret_cast<const uint8_t*>(elf.data()), elf.size());

    ASSERT_THROW(VPUXLoader(&accessor, &bufMgr), ImplausibleState);
}

TEST(ELFLoader, NoThrowWhenUnrecognizedUserSectionType) {
    DummyBufferManager bufMgr;
    std::vector<uint8_t> elf;

    auto blobConfig = ActionsSequence{{AddDummyBinarySection::build(
            ".binSection_0",
            AddDummyBinarySection::Attributes{{}, {SHT_HIUSER - 1, std::vector<DummyBinObject>(24)}})}};

    OV_ASSERT_NO_THROW(elf = TestBlob(blobConfig).getBinary(););

    DDRAccessManager<elf::DDRAlwaysEmplace> accessor(reinterpret_cast<const uint8_t*>(elf.data()), elf.size());
    VPUXLoader loader(&accessor, &bufMgr);
    OV_ASSERT_NO_THROW(loader.load(gSymTab.symTab()));
}

TEST(ELFLoader, NoThrowWhenValidElf) {
    DummyBufferManager bufMgr;
    std::vector<uint8_t> elf;

    OV_ASSERT_NO_THROW(elf = TestBlob(validElfDefault).getBinary(););

    DDRAccessManager<elf::DDRAlwaysEmplace> accessor(reinterpret_cast<const uint8_t*>(elf.data()), elf.size());
    OV_ASSERT_NO_THROW(VPUXLoader(&accessor, &bufMgr));
    VPUXLoader loader(&accessor, &bufMgr);
    OV_ASSERT_NO_THROW(loader.load(gSymTab.symTab(), false, {}, false));
}

TEST(ELFLoader, NoThrowForDmaRelocations) {
    DummyBufferManager bufMgr;
    std::vector<uint8_t> elf;

    OV_ASSERT_NO_THROW(elf = TestBlob(validElfWithDmaRelocations).getBinary(););

    DDRAccessManager<elf::DDRAlwaysEmplace> accessor(reinterpret_cast<const uint8_t*>(elf.data()), elf.size());
    // OV_ASSERT_NO_THROW(VPUXLoader(&accessor, &bufMgr));
    VPUXLoader loader(&accessor, &bufMgr);
    OV_ASSERT_NO_THROW(loader.load(gSymTab.symTab(), false, {}));

    uint8_t data = 0;

    elf::DeviceBuffer input(&data, 0xB, 0xB);
    std::vector<elf::DeviceBuffer> inputs{input};
    OV_ASSERT_NO_THROW(loader.applyJitRelocations(inputs, inputs, inputs));
}

TEST(ELFLoader, NoThrowForDmaRelocationsWithUserStrides) {
    DummyBufferManager bufMgr;
    std::vector<uint8_t> elf;

    OV_ASSERT_NO_THROW(elf = TestBlob(validElfWithDmaRelocations).getBinary(););

    DDRAccessManager<elf::DDRAlwaysEmplace> accessor(reinterpret_cast<const uint8_t*>(elf.data()), elf.size());
    OV_ASSERT_NO_THROW(VPUXLoader(&accessor, &bufMgr));
    VPUXLoader loader(&accessor, &bufMgr);
    OV_ASSERT_NO_THROW(loader.load(gSymTab.symTab(), false, {}));

    uint8_t data = 0;

    elf::DeviceBuffer input(&data, 0xB, 0xB);
    input.set_user_strides({1, 12, 24, 96, 0});
    std::vector<elf::DeviceBuffer> inputs{input};
    OV_ASSERT_NO_THROW(loader.applyJitRelocations(inputs, inputs, inputs));
}

TEST(ELFLoader, SimpleMetadata) {
    // Metadata built sequentially
    auto metadata0 = elf::NetworkMetadata{{"Test identification", "Test blob"}};
    auto serializedMetadata0 = elf::MetadataSerialization::serialize(metadata0);

    auto metadataAction0 = AddRawBinarySection::build(
            ".metadata", AddRawBinarySection::Attributes{{}, {VPU_SHT_NETDESC, std::move(serializedMetadata0)}});

    auto testBlob0 = TestBlob({{metadataAction0}});
    auto elf0 = testBlob0.getBinary();

    // Metadata built in one shot
    auto elf1 = TestBlob({{AddRawBinarySection::build(
                                 ".metadata",
                                 AddRawBinarySection::Attributes{
                                         {},
                                         {
                                                 VPU_SHT_NETDESC,
                                                 elf::MetadataSerialization::serialize(
                                                         elf::NetworkMetadata{{"Test identification", "Test blob"}}),
                                         }})}})
                        .getBinary();

    // Blobs must be identical, even though build through different APIs
    ASSERT_EQ(elf0, elf1);

    auto accessor0 =
            DDRAccessManager<elf::DDRAlwaysEmplace>(reinterpret_cast<const uint8_t*>(elf0.data()), elf0.size());
    auto bufferManager0 = DummyBufferManager();
    auto loader0 = VPUXLoader(&accessor0, &bufferManager0);

    auto metadataSections0 = loader0.getSectionsOfType(VPU_SHT_NETDESC);

    ASSERT_EQ(metadataSections0.size(), 1);

    auto metadataBuffer0 = metadataSections0.front()->getBuffer();
    auto readbackMetadata0 =
            elf::MetadataSerialization::deserialize(metadataBuffer0.cpu_addr(), metadataBuffer0.size());

    // Original metadata and deserialized metadata must be identical
    ASSERT_EQ(metadata0, *readbackMetadata0);

    auto accessor1 =
            DDRAccessManager<elf::DDRAlwaysEmplace>(reinterpret_cast<const uint8_t*>(elf1.data()), elf1.size());
    auto bufferManager1 = DummyBufferManager();
    auto loader1 = VPUXLoader(&accessor1, &bufferManager1);

    auto metadataSections1 = loader1.getSectionsOfType(VPU_SHT_NETDESC);

    auto metadataBuffer1 = metadataSections1.front()->getBuffer();
    auto readbackMetadata1 =
            elf::MetadataSerialization::deserialize(metadataBuffer1.cpu_addr(), metadataBuffer1.size());

    ASSERT_EQ(*readbackMetadata0, *readbackMetadata1);

    metadata0.mIdentification.arch_name[0] ^= metadata0.mIdentification.arch_name[0];
    ASSERT_NE(metadata0, *readbackMetadata0);
}

TEST(ELFLoader, TwoMetadataSections) {
    auto elf =
            TestBlob({{

                             AddRawBinarySection::build(
                                     ".metadata0",
                                     AddRawBinarySection::Attributes{
                                             {},
                                             {
                                                     VPU_SHT_NETDESC,
                                                     std::move(
                                                             elf::MetadataSerialization::serialize(elf::NetworkMetadata{
                                                                     {"Test identification", "Test blob"}})),
                                             }}),

                             AddRawBinarySection::build(
                                     ".metadata1",
                                     AddRawBinarySection::Attributes{
                                             {},
                                             {
                                                     VPU_SHT_NETDESC,
                                                     std::move(
                                                             elf::MetadataSerialization::serialize(elf::NetworkMetadata{
                                                                     {"Test identification", "Test blob"}})),
                                             }})

                     }})
                    .getBinary();

    auto accessor = DDRAccessManager<elf::DDRAlwaysEmplace>(reinterpret_cast<const uint8_t*>(elf.data()), elf.size());
    auto bufferManager = DummyBufferManager();
    auto loader = VPUXLoader(&accessor, &bufferManager);

    auto metadataSections = loader.getSectionsOfType(VPU_SHT_NETDESC);

    ASSERT_EQ(metadataSections.size(), 2);
}

TEST(ELFLoader, ThrowForDmaRelocationsWritingOverflow) {
    auto elf =
            TestBlob(ActionsSequence{{

                             AddDMADescriptorBinarySection::build(
                                     ".binSection_0",
                                     AddDMADescriptorBinarySection::Attributes{
                                             {},
                                             // Single DmaDescriptor
                                             {elf::SHT_PROGBITS, AddDMADescriptorBinarySection::Vector(1)}}),

                             AddDMASymbolSection::build(
                                     ".symtab_0", AddDMASymbolSection::Attributes{VPU_SHF_USERINPUT | VPU_SHF_JIT},
                                     ActionsSequence{{

                                             AddDMASymbol::build(
                                                     "symtab_0_sym0",
                                                     AddDMASymbol::Attributes{DmaSymbolEntry{0,
                                                                                             0,
                                                                                             {6, 2, 4, 0, 0, 0},
                                                                                             {1, 6, 12, 48, 0, 0},
                                                                                             {0, 0, 0, 0, 0, 0},
                                                                                             {6, 2, 4, 0, 0, 0},
                                                                                             {1, 6, 12, 48, 0, 0},
                                                                                             4}},
                                                     AddDMASymbol::Operands{}),
                                     }}),

                             AddDMARelocationSection::build(
                                     ".relocSection_0",
                                     AddDMARelocationSection::Attributes{VPU_SHF_USERINPUT | VPU_SHF_JIT},
                                     AddDMARelocationSection::Operands{".symtab_0", ".binSection_0"},
                                     ActionsSequence{{

                                             AddDMARelocation::build(
                                                     "reloc1",
                                                     AddDMARelocation::Attributes{
                                                             R_VPU_DMA_TASK_OUTPUT,
                                                             // Since target section only contains 1 DmaDescriptor and
                                                             // relocation size is equal to size of DmaDescriptor, any
                                                             // offset other than 0 should trigger an error
                                                             1, 0},
                                                     AddDMARelocation::Operands{"symtab_0_sym0"})

                                     }}),

                     }})
                    .getBinary();

    auto accessor = DDRAccessManager<elf::DDRAlwaysEmplace>(reinterpret_cast<const uint8_t*>(elf.data()), elf.size());
    auto bufferManager = DummyBufferManager();
    auto loader = VPUXLoader(&accessor, &bufferManager);

    OV_ASSERT_NO_THROW(loader.load(gSymTab.symTab(), false, {}));

    uint8_t data = 0;
    elf::DeviceBuffer input(&data, 0xB, 0xB);
    std::vector<elf::DeviceBuffer> inputs{input};
    ASSERT_THROW(loader.applyJitRelocations(inputs, inputs, inputs), RelocError);
}

using RelocationTestParams = std::tuple<ActionsSequence, ActionsSequence>;

static const auto RelocationTestPrerequisites = std::vector<ActionsSequence>{ActionsSequence{{

        // !!! Some relocs make use of data from target location. For now, all 0 data works just fine for
        // all relocs,
        // but pay attention to potential seg faults when adding new relocs. In that case, target section
        // data needs to
        // be adjusted such that applying the relocation is a valid action.
        AddRawBinarySection::build(
                ".binSection_0",
                AddRawBinarySection::Attributes{{}, {elf::SHT_PROGBITS, AddRawBinarySection::Vector(1024)}}),

        AddSymbolSection::build(".symtab_0", AddSymbolSection::Attributes{},
                                ActionsSequence{{

                                        AddSymbol::build("symtab_0_sym0", AddSymbol::Attributes{STT_SECTION},
                                                         AddSymbol::Operands{".binSection_0"})

                                }}),

        AddRelocationSection::build(".relocSection_0",
                                    AddRelocationSection::Attributes{{VPU_SHF_USERINPUT | VPU_SHF_JIT, 8}},
                                    AddRelocationSection::Operands{".symtab_0", ".binSection_0"}, ActionsSequence{})}}

};

static const auto DMARelocationTestPrerequisites = std::vector<ActionsSequence>{ActionsSequence{{

        // !!! Some relocs make use of data from target location. For now, all 0 data works just fine for
        // all relocs,
        // but pay attention to potential seg faults when adding new relocs. In that case, target section
        // data needs to
        // be adjusted such that applying the relocation is a valid action.
        AddDMADescriptorBinarySection::build(
                ".binSection_0",
                AddDMADescriptorBinarySection::Attributes{
                        {}, {elf::SHT_PROGBITS, AddDMADescriptorBinarySection::Vector(8)}}),

        AddDMASymbolSection::build(
                ".symtab_0", AddDMASymbolSection::Attributes{{}},
                ActionsSequence{{

                        AddDMASymbol::build("symtab_0_sym0", AddDMASymbol::Attributes{}, AddDMASymbol::Operands{""})

                }}),

        AddDMARelocationSection::build(
                ".relocSection_0", AddDMARelocationSection::Attributes{{VPU_SHF_USERINPUT | VPU_SHF_JIT}},
                AddDMARelocationSection::Operands{".symtab_0", ".binSection_0"}, ActionsSequence{})}}

};

static const auto RelocationTestRelocations = std::vector<ActionsSequence>{

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_64, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_16_SUM, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_64_MULT, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_64_MULT_SUB, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_64_OR, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_DISP40_RTM, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_64_LSHIFT, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_32, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_32_RTM, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_32_SUM, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_32_MULTICAST_BASE, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_32_MULTICAST_BASE_SUB, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_DISP28_MULTICAST_OFFSET, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{
                {AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_DISP4_MULTICAST_OFFSET_CMP, 0, 0},
                                      AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_LO_21, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_LO_21_SUM, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_LO_21_MULTICAST_BASE, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_16_LSB_21_RSHIFT_5, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_LO_21_RSHIFT_4, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_CMX_LOCAL_RSHIFT_5, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_32_BIT_OR_B21_B26_UNSET, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_64_BIT_OR_B21_B26_UNSET, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{
                {AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_16_LSB_21_RSHIFT_5_LSHIFT_16, 0, 0},
                                      AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{
                {AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_16_LSB_21_RSHIFT_5_LSHIFT_CUSTOM, 0, 0},
                                      AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{
                {AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_32_BIT_OR_B21_B26_UNSET_HIGH_16, 0, 0},
                                      AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{
                {AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_32_BIT_OR_B21_B26_UNSET_LOW_16, 0, 0},
                                      AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_HIGH_27_BIT_OR, 0, 0},
                                              AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{
                {AddRelocation::build("reloc0", AddRelocation::Attributes{R_VPU_32_OR_LO_19_LSB_21_RSHIFT_2, 0, 0},
                                      AddRelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

};

static const auto DMARelocationTestRelocations = std::vector<ActionsSequence>{

        ActionsSequence{{AddDMARelocation::build("reloc0", AddDMARelocation::Attributes{R_VPU_DMA_TASK_INPUT, 0, 0},
                                                 AddDMARelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

        ActionsSequence{{AddDMARelocation::build("reloc0", AddDMARelocation::Attributes{R_VPU_DMA_TASK_OUTPUT, 0, 0},
                                                 AddDMARelocation::Operands{"symtab_0_sym0", ".relocSection_0"})}},

};

class ELFLoaderRelocationTest : public testing::TestWithParam<RelocationTestParams> {};

TEST_P(ELFLoaderRelocationTest, ) {
    auto [prereqs, relocs] = GetParam();

    auto testBlob = TestBlob();

    testBlob.execute(prereqs);
    testBlob.execute(relocs);

    auto elf = testBlob.getBinary();

    auto accessor = DDRAccessManager<elf::DDRAlwaysEmplace>(reinterpret_cast<const uint8_t*>(elf.data()), elf.size());
    auto bufferManager = DummyBufferManager();
    ASSERT_NO_THROW(VPUXLoader(&accessor, &bufferManager).load({}));
}

INSTANTIATE_TEST_SUITE_P(RelocationPackTests, ELFLoaderRelocationTest,
                         testing::Combine(testing::ValuesIn(RelocationTestPrerequisites),
                                          testing::ValuesIn(RelocationTestRelocations)));

INSTANTIATE_TEST_SUITE_P(DMARelocationPackTests, ELFLoaderRelocationTest,
                         testing::Combine(testing::ValuesIn(DMARelocationTestPrerequisites),
                                          testing::ValuesIn(DMARelocationTestRelocations)));

}  // namespace
