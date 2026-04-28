//
// Copyright (C) 2026 Intel Corporation.
// SPDX-License-Identifier: Apache-2.0
//

#include <gtest/gtest.h>
#include <chrono>
#include <cstddef>
#include <ctime>

#include "allocator_utils/blob_scanner.hpp"
#include "allocator_utils/buffer_managers.hpp"
#include "allocator_utils/hpi_runner.hpp"
#include "allocator_utils/io_container.hpp"
#include "test_blob/binary_actions.hpp"
#include "test_blob/symbol_actions.hpp"
#include "test_blob/test_blob.hpp"

#include "hpi_common_interface.hpp"
#include "vpux_elf/accessor.hpp"
#include "vpux_elf/types/section_header.hpp"
#include "vpux_elf/types/vpu_extensions.hpp"
#include "vpux_headers/metadata.hpp"
#include "vpux_headers/platform.hpp"
#include "vpux_headers/serial_metadata.hpp"
#include "vpux_hpi.hpp"

using namespace elf;
using namespace std;
using namespace chrono;

namespace {

enum class RunMode {
    SimpleLoad = 0,
    SimpleClone,
};

}  // namespace

using AddNoteBinarySection = AddBinarySectionAction<elf::elf_note::VersionNote>;

struct SimpleLoadRunner : public HPIRunner<SimpleLoadRunner> {
    SimpleLoadRunner(): HPIRunner("appArgArchName", "appArgBlobPathAndName", AccessManagerType::DDRAccessManager) {
    }

    void runImpl() {
        // Get a memory consumption projection from the blob
        BlobScanner blobScanner(_accessManager.get(), getDefaultProcessorMap());
        blobScanner.printResult();

        std::cout << "\nLoading HPI..." << std::endl;

        auto start = high_resolution_clock::now();

        HostParsedInference hpi(_hpiBufferManager.get(), _accessManager.get(), _hpiConfig);
        hpi.load();

        auto end = high_resolution_clock::now();

        std::cout << "HPI loaded in " << duration_cast<milliseconds>(end - start).count() << " ms\n" << std::endl;

        IOBuffersContainer ioContainer(_ioBufferManager, hpi.getInputBuffers(), hpi.getOutputBuffers(),
                                       hpi.getProfBuffers());

        hpi.applyInputOutput(ioContainer.getInputBuffers(), ioContainer.getOutputBuffers(),
                             ioContainer.getProfilingBuffers());

        std::cout << "Projected NPU memory for HPI: "
                  << blobScanner.getRequirementsByAllocationType().getTotalRequired() + hpi.getHPISize() << " bytes\n";
        std::cout << "Total NPU buffers tracked by HPI object: " << hpi.getAllocatedBuffers().size() << "\n";
        std::cout << std::endl;

        _hpiBufferManager->printAllocationStats();
    }
};

class SimpleCloneRunner : public HPIRunner<SimpleCloneRunner> {
public:
    SimpleCloneRunner(): HPIRunner("appArgArchName", "appArgBlobPathAndName", AccessManagerType::DDRAccessManager) {
    }

    void runImpl() {
        std::cout << "Loading first HPI..." << std::endl;
        HostParsedInference hpi(_hpiBufferManager.get(), _accessManager.get(), _hpiConfig);
        hpi.load();
        std::cout << "First HPI loaded\n" << std::endl;

        // Delete AccessManager
        _accessManager = nullptr;

        // IO bindings must be possible after blob release
        IOBuffersContainer ioContainer(_ioBufferManager, hpi.getInputBuffers(), hpi.getOutputBuffers(),
                                       hpi.getProfBuffers());
        hpi.applyInputOutput(ioContainer.getInputBuffers(), ioContainer.getOutputBuffers(),
                             ioContainer.getProfilingBuffers());

        _hpiBufferManager->printAllocationStats();

        std::cout << "Loading second HPI..." << std::endl;

        // Cloning must still work after AccessManager was deleted
        HostParsedInference hpiClone(hpi);

        std::cout << "Second HPI loaded\n" << std::endl;

        _hpiBufferManager->printAllocationStats();
        hpiClone.getMetadata();
    }
};

TEST(HostParsedInference, MetadataOnlyBlobIsRejected) {
    auto elf = TestBlob({{AddRawBinarySection::build(
                                ".metadata",
                                AddRawBinarySection::Attributes{
                                        {},
                                        {
                                                VPU_SHT_NETDESC,
                                                elf::MetadataSerialization::serialize(
                                                        elf::NetworkMetadata{{"Test identification", "Test blob"}}),
                                        }})}})
                       .getBinary();

    auto accessManager =
            DDRAccessManager<elf::DDRAlwaysEmplace>(reinterpret_cast<const uint8_t*>(elf.data()), elf.size());
    auto bufferManager = DummyBufferManager();

    ASSERT_THROW(elf::HostParsedInference(&bufferManager, &accessManager, HPIConfigs{}, nullptr), std::exception);
}

const auto MinimalConstructible40XX = ActionsSequence{{

        AddNoteBinarySection::build(
                ".ELFVersion",
                AddNoteBinarySection::Attributes{
                        {},
                        {elf::SHT_NOTE,
                         std::vector<elf::elf_note::VersionNote>{elf::elf_note::VersionNote{
                                 0,
                                 0,
                                 elf::elf_note::NT_GNU_ABI_TAG,
                                 {},
                                 elf::HostParsedInferenceCommon::getArchSpecificHPI(elf::platform::ArchKind::VPUX40XX)
                                         ->getELFLibABIVersion()
                                         .getMIFormat(),
                                 elf::HostParsedInferenceCommon::getArchSpecificHPI(elf::platform::ArchKind::VPUX40XX)
                                         ->getELFLibABIVersion()
                                         .getMajor(),
                                 elf::HostParsedInferenceCommon::getArchSpecificHPI(elf::platform::ArchKind::VPUX40XX)
                                         ->getELFLibABIVersion()
                                         .getMinor(),
                                 elf::HostParsedInferenceCommon::getArchSpecificHPI(elf::platform::ArchKind::VPUX40XX)
                                         ->getELFLibABIVersion()
                                         .getPatch()}}},
                }),

        AddNoteBinarySection::build(
                ".MIVersion",
                AddNoteBinarySection::Attributes{
                        {},
                        {elf::SHT_NOTE,
                         std::vector<elf::elf_note::VersionNote>{elf::elf_note::VersionNote{
                                 0,
                                 0,
                                 elf::elf_note::NT_NPU_MPI_VERSION,
                                 {},
                                 elf::HostParsedInferenceCommon::getArchSpecificHPI(elf::platform::ArchKind::VPUX40XX)
                                         ->getStaticMIVersion()
                                         .getMIFormat(),
                                 elf::HostParsedInferenceCommon::getArchSpecificHPI(elf::platform::ArchKind::VPUX40XX)
                                         ->getStaticMIVersion()
                                         .getMajor(),
                                 elf::HostParsedInferenceCommon::getArchSpecificHPI(elf::platform::ArchKind::VPUX40XX)
                                         ->getStaticMIVersion()
                                         .getMinor(),
                                 elf::HostParsedInferenceCommon::getArchSpecificHPI(elf::platform::ArchKind::VPUX40XX)
                                         ->getStaticMIVersion()
                                         .getPatch()}}},
                }),

        AddRawBinarySection::build(
                ".metadata",
                AddRawBinarySection::Attributes{{},
                                                {
                                                        VPU_SHT_NETDESC,
                                                        elf::MetadataSerialization::serialize(elf::NetworkMetadata{
                                                                {"Test identification", "Test blob"}}),
                                                }}),

        AddRawBinarySection::build(".platformInfo",
                                   AddRawBinarySection::Attributes{
                                           {},
                                           {
                                                   VPU_SHT_PLATFORM_INFO,
                                                   elf::platform::PlatformInfoSerialization::serialize(
                                                           elf::platform::PlatformInfo{platform::ArchKind::VPUX40XX}),
                                           }}),
}};

const auto MinimalLoadable40XX =
        MinimalConstructible40XX +
        ActionsSequence{{AddRawBinarySection::build(
                                 ".mappedInference",
                                 AddRawBinarySection::Attributes{{elf::SHF_ALLOC | elf::SHF_EXECINSTR},
                                                                 {elf::SHT_PROGBITS, std::vector<uint8_t>(64)}}),

                         AddSymbolSection::build(
                                 ".symtab", AddSymbolSection::Attributes{},
                                 ActionsSequence{{AddSymbol::build(".entry", AddSymbol::Attributes{elf::VPU_STT_ENTRY},
                                                                   AddSymbol::Operands{".mappedInference"})}})}};

TEST(HostParsedInference, MinimalConstructible) {
    auto arch = elf::platform::ArchKind::VPUX40XX;
    auto archSpecHpi = elf::HostParsedInferenceCommon::getArchSpecificHPI(arch);

    auto elf = TestBlob(MinimalConstructible40XX).getBinary();

    auto accessManager =
            DDRAccessManager<elf::DDRAlwaysEmplace>(reinterpret_cast<const uint8_t*>(elf.data()), elf.size());
    auto bufferManager = DummyBufferManager();

    ASSERT_NO_THROW(elf::HostParsedInference(&bufferManager, &accessManager,
                                             HPIConfigs{{}, elf::platform::ArchKind::VPUX40XX}, nullptr));
}

TEST(HostParsedInference, MinimalLoadable) {
    auto arch = elf::platform::ArchKind::VPUX40XX;
    auto archSpecHpi = elf::HostParsedInferenceCommon::getArchSpecificHPI(arch);

    auto elf = TestBlob(MinimalLoadable40XX).getBinary();

    auto bufferManager = HeapBufferManager();
    auto accessManager = DDRAccessManager<elf::DDRNeverEmplace, elf::AllocatedDeviceBufferFactory>(
            reinterpret_cast<const uint8_t*>(elf.data()), elf.size(),
            std::make_shared<elf::AllocatedDeviceBufferFactory>(&bufferManager));

    auto hpi = elf::HostParsedInference(&bufferManager, &accessManager, HPIConfigs{{}, arch}, nullptr);

    ASSERT_NO_THROW(hpi.load());
}

TEST(HostParsedInference, BaseMemoryCheck) {
    auto arch = elf::platform::ArchKind::VPUX40XX;
    auto archSpecHpi = elf::HostParsedInferenceCommon::getArchSpecificHPI(arch);

    auto elf = TestBlob(MinimalLoadable40XX).getBinary();

    auto bufferManager = HeapBufferManager();
    auto accessManager = DDRAccessManager<elf::DDRNeverEmplace, elf::AllocatedDeviceBufferFactory>(
            reinterpret_cast<const uint8_t*>(elf.data()), elf.size(),
            std::make_shared<elf::AllocatedDeviceBufferFactory>(&bufferManager));

    auto blobScanner = BlobScanner(&accessManager, getDefaultProcessorMap());
    auto procReq = blobScanner.getRequirementsByProcessor().getTotalRequired() +
                   archSpecHpi->getParsedInferenceBufferSpecs().size;

    auto hpi = elf::HostParsedInference(&bufferManager, &accessManager, HPIConfigs{{}, arch}, nullptr);

    ASSERT_NO_THROW(hpi.load());

    auto procAlloc = bufferManager.getStats()._totalNPUSize;

    ASSERT_EQ(procReq, procAlloc);
}

TEST(HostParsedInference, ScratchSectionHasNoImpactOnBlobSize) {
    auto arch = elf::platform::ArchKind::VPUX40XX;
    auto archSpecHpi = elf::HostParsedInferenceCommon::getArchSpecificHPI(arch);

    auto seq0 = MinimalLoadable40XX +
                ActionsSequence{{

                        AddEmptySection::build(
                                ".buffer", AddEmptySection::Attributes{{elf::SHF_ALLOC | elf::VPU_SHF_PROC_DMA, 1024},
                                                                       4 * 1024 * 1024 * 1024ULL})

                }};

    auto seq1 = MinimalLoadable40XX +
                ActionsSequence{{

                        AddEmptySection::build(
                                ".buffer", AddEmptySection::Attributes{{elf::SHF_ALLOC | elf::VPU_SHF_PROC_DMA, 1024},
                                                                       2 * 1024 * 1024 * 1024ULL})}};

    auto elf0 = TestBlob(seq0).getBinary();
    auto elf1 = TestBlob(seq1).getBinary();

    ASSERT_EQ(elf0.size(), elf1.size());

    const auto loadAndReturnMemUsage = [&arch](const std::vector<uint8_t>& elf) -> size_t {
        auto bufferManager = HeapBufferManager();
        auto accessManager = DDRAccessManager<elf::DDRNeverEmplace, elf::AllocatedDeviceBufferFactory>(
                reinterpret_cast<const uint8_t*>(elf.data()), elf.size(),
                std::make_shared<elf::AllocatedDeviceBufferFactory>(&bufferManager));

        auto hpi = elf::HostParsedInference(&bufferManager, &accessManager, HPIConfigs{{}, arch}, nullptr);
        hpi.load();

        return bufferManager.getStats()._totalNPUSize;
    };

    size_t memUsage0 = 0;
    size_t memUsage1 = 0;

    ASSERT_NO_THROW(memUsage0 = loadAndReturnMemUsage(elf0));
    ASSERT_NO_THROW(memUsage1 = loadAndReturnMemUsage(elf1));

    ASSERT_GT(memUsage0, memUsage1);
}
