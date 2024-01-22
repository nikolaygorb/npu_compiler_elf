//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#ifndef VPUX_ELF_LOG_UNIT_NAME
#define VPUX_ELF_LOG_UNIT_NAME "VpuxHpi"
#endif
// clang-format off
#include <vpux_loader/vpux_loader.hpp>
#include <vpux_elf/utils/log.hpp>
#include <vpux_hpi.hpp>
#include <sstream>

#if defined(CONFIG_TARGET_SOC_3720) || defined(HOST_BUILD)
#include <hpi_3720.hpp>
#endif

#if defined(CONFIG_TARGET_SOC_4000) || defined(HOST_BUILD)
#include <hpi_4000.hpp>
#endif

//to be removed with E#88139:
//temporary fix to support NPU 5000 arch
#if defined(CONFIG_TARGET_SOC_5000) || defined(HOST_BUILD)
#include <hpi_4000.hpp>
#endif

#include <string.h>
// clang-format on

namespace elf {

enum ArchKind { UNKNOWN = 0, VPUX37XX, VPUX40XX };

namespace {

const elf::ElfVersion parseAbiVersionStruct(const elf::elf_note::Elf_AbiVersionNote& abiVersionStruct) {
    return {abiVersionStruct.n_desc[1], abiVersionStruct.n_desc[2], abiVersionStruct.n_desc[3]};
}

void checkELFLibABICompatibility(const elf::ElfVersion loaderABIVersion, const elf::ElfVersion elfABIVersion) {
    std::ostringstream loaderABIVersionStream;
    loaderABIVersionStream << loaderABIVersion.major << "." << loaderABIVersion.minor << "." << loaderABIVersion.patch;

    std::ostringstream elfABIVersionStream;
    elfABIVersionStream << elfABIVersion.major << "." << elfABIVersion.minor << "." << elfABIVersion.patch;

    if (loaderABIVersion.major != elfABIVersion.major || loaderABIVersion.minor < elfABIVersion.minor) {
        VPUX_ELF_LOG(LogLevel::LOG_ERROR, "ELF Library ABI Version is not compatible with the ELF");
        VPUX_ELF_LOG(LogLevel::LOG_ERROR, "\tExpected ABI Version: %s and received ELF ABI Version: %s",
                     loaderABIVersionStream.str().c_str(), elfABIVersionStream.str().c_str());

        std::ostringstream errorMsgStream;
        errorMsgStream << "Versioning Error. ELF Library ABI Versions are incompatible. Provided: "
                       << elfABIVersionStream.str() << " vs Expected: " << loaderABIVersionStream.str();
        VPUX_ELF_THROW(VersioningError, errorMsgStream.str().c_str(), elfABIVersion, loaderABIVersion);
    } else if (loaderABIVersion.minor > elfABIVersion.minor) {
        VPUX_ELF_LOG(LogLevel::LOG_WARN, "Warning! ELF Library ABI Versions are compatible but do not match.");
        VPUX_ELF_LOG(LogLevel::LOG_WARN, "\tExpected ABI Version: %s and eceived ELF ABI Version: %s",
                     loaderABIVersionStream.str().c_str(), elfABIVersionStream.str().c_str());
    } else {
        VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "ELF Library ABI Versions are perfectly compatible. Version: %s",
                     loaderABIVersionStream.str().c_str());
    }
}

const static std::unordered_map<std::string, ArchKind> knownArch = {{"VPUX37XX", ArchKind::VPUX37XX},
                                                                    {"VPUX40XX", ArchKind::VPUX40XX},
                                                                    // to be removed with E#88139:
                                                                    // temporary fix to support NPU 5000 arch
                                                                    {"VPUX50XX", ArchKind::VPUX40XX}};

static ArchKind mapArchStringToArchKind(const std::string& archName) {
    auto retArch = knownArch.find(archName);
    if (retArch != knownArch.end()) {
        return retArch->second;
    } else {
        return ArchKind::UNKNOWN;
    }
}

static std::unique_ptr<HostParsedInferenceCommon> getArchSpecificHPI(const std::string& archName) {
    auto arch = mapArchStringToArchKind(archName);

    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "Creating specialized HPI for arch %u", arch);

    std::unique_ptr<HostParsedInferenceCommon> archSpecificHPI;
    switch (arch) {
#if defined(CONFIG_TARGET_SOC_3720) || defined(HOST_BUILD)
    case ArchKind::VPUX37XX:
        archSpecificHPI = std::make_unique<HostParsedInference_3720>();
        break;
#endif

// to be updated with E#88139:
// temporary fix to support NPU 5000 arch
#if defined(CONFIG_TARGET_SOC_4000) || (CONFIG_TARGET_SOC_5000) || defined(HOST_BUILD)
    case ArchKind::VPUX40XX:
        archSpecificHPI = std::make_unique<HostParsedInference_4000>();
        break;
#endif
    default:
        VPUX_ELF_THROW(RangeError, (archName + " arch is not supported").c_str());
        break;
    }

    return archSpecificHPI;
}

}  // namespace

const uint64_t* HostParsedInference::readPerfMetrics() {
    const auto& sections = loader->getSectionsOfType(elf::VPU_SHT_PERF_METRICS);
    VPUX_ELF_THROW_WHEN(sections.size() > 1, RangeError, "Expected only a single section of performance metrics.");

    if (sections.size() == 1) {
        return reinterpret_cast<const uint64_t*>(sections[0].cpu_addr());
    }

    VPUX_ELF_LOG(LogLevel::LOG_WARN, "No performance metrics. Default to be used!");
    return nullptr;
}

void HostParsedInference::readMetadata() {
    const auto& sections = loader->getSectionsOfType(elf::VPU_SHT_NETDESC);
    VPUX_ELF_THROW_UNLESS(sections.size() == 1, RangeError, "Expected only one metadata section.");

    auto metadataBufferPtr = sections[0].cpu_addr();
    auto metadataBufferSize = sections[0].size();
    metadata = MetadataSerialization::deserialize(metadataBufferPtr, metadataBufferSize);
}

elf::ElfVersion HostParsedInference::readElfABIVersion() const {
    const auto& sections = loader->getSectionsOfType(elf::SHT_NOTE);
    for(auto i:sections)
    {
        elf::elf_note::Elf_AbiVersionNote elfABIVersionNote{};
        VPUX_ELF_THROW_UNLESS(i.size() == sizeof(elfABIVersionNote), SectionError, "Wrong ABI size.");
        memcpy(&elfABIVersionNote, i.cpu_addr(), sizeof(elfABIVersionNote));
        if(elfABIVersionNote.n_type == elf::elf_note::NT_GNU_ABI_TAG)
        {
            return parseAbiVersionStruct(elfABIVersionNote);
        }
    }
    VPUX_ELF_THROW(RangeError, "Expected ABI version not found.");
}

HostParsedInference::HostParsedInference(BufferManager* bufferMgr, AccessManager* accessMgr)
        : bufferManager(bufferMgr), accessManager(accessMgr) {
    loader = std::make_unique<VPUXLoader>(accessMgr, bufferMgr);
    readMetadata();
}

void HostParsedInference::load() {
    auto archName = std::string(metadata->mIdentification.arch_name);

    // TODO: E#79344
    auto archSpecificHpi = getArchSpecificHPI(archName);
    checkELFLibABICompatibility(archSpecificHpi->getELFLibABIVersion(), readElfABIVersion());
    // TODO define readMIVersion based on readElfABIVersion
    // checkELFLibABICompatibility(archSpecificHpi->getExpectedMIVersion(), readMIVersion());


    const auto symbolTable = archSpecificHpi->getSymbolTable(metadata->mResourceRequirements.nn_slice_count_);
    const auto symbolSectionTypes = archSpecificHpi->getSymbolSectionTypes();
    auto symTabOverrideMode = archSpecificHpi->getSymbolSectionTypes().size() == 0 ? false : true;

    loader->load(symbolTable, symTabOverrideMode, symbolSectionTypes);

    parsedInference =
            std::make_shared<AllocatedDeviceBuffer>(bufferManager, archSpecificHpi->getParsedInferenceBufferSpecs());
    auto parsedInferenceBuffer = parsedInference->getBuffer();
    archSpecificHpi->setHostParsedInference(parsedInferenceBuffer, loader->getEntry(), metadata->mResourceRequirements,
                                            readPerfMetrics());
}

HostParsedInference::HostParsedInference(const HostParsedInference& other)
        : bufferManager(other.bufferManager), accessManager(other.accessManager), metadata(other.metadata) {
    auto archName = std::string(metadata->mIdentification.arch_name);

    // TODO: E#79344
    auto archSpecificHpi = getArchSpecificHPI(archName);
    // Use clone semantics here by copy-constructing the loader object
    loader = std::make_unique<VPUXLoader>(*other.loader);
    // Every new loader object means a new parsedInference struct as well
    parsedInference =
            std::make_shared<AllocatedDeviceBuffer>(bufferManager, archSpecificHpi->getParsedInferenceBufferSpecs());
    auto parsedInferenceBuffer = parsedInference->getBuffer();
    archSpecificHpi->setHostParsedInference(parsedInferenceBuffer, loader->getEntry(), metadata->mResourceRequirements,
                                            readPerfMetrics());
};

HostParsedInference::HostParsedInference(HostParsedInference&& other)
        : bufferManager(other.bufferManager),
          accessManager(other.accessManager),
          metadata(other.metadata),
          loader(std::move(other.loader)),
          parsedInference(other.parsedInference) {
}

HostParsedInference::~HostParsedInference() {
}

HostParsedInference& HostParsedInference::operator=(const HostParsedInference& rhs) {
    if (this == &rhs) {
        return *this;
    }

    bufferManager = rhs.bufferManager;
    accessManager = rhs.accessManager;
    metadata = rhs.metadata;

    auto archName = std::string(metadata->mIdentification.arch_name);

    // TODO: E#79344
    auto archSpecificHpi = getArchSpecificHPI(archName);
    // Use clone semantics here by copy-constructing the loader object
    loader = std::make_unique<VPUXLoader>(*rhs.loader);
    // Every new loader object means a new parsedInference struct as well
    parsedInference =
            std::make_shared<AllocatedDeviceBuffer>(bufferManager, archSpecificHpi->getParsedInferenceBufferSpecs());
    auto parsedInferenceBuffer = parsedInference->getBuffer();
    archSpecificHpi->setHostParsedInference(parsedInferenceBuffer, loader->getEntry(), metadata->mResourceRequirements,
                                            readPerfMetrics());

    return *this;
}

HostParsedInference& HostParsedInference::operator=(HostParsedInference&& rhs) {
    if (this == &rhs) {
        return *this;
    }
    bufferManager = rhs.bufferManager;
    accessManager = rhs.accessManager;
    metadata = rhs.metadata;
    loader = std::move(rhs.loader);
    parsedInference = rhs.parsedInference;

    return *this;
}

DeviceBuffer HostParsedInference::getParsedInference() const {
    return parsedInference->getBuffer();
}

std::vector<DeviceBuffer> HostParsedInference::getAllocatedBuffers() const {
    return loader->getAllocatedBuffers();
}

std::vector<DeviceBuffer> HostParsedInference::getInputBuffers() const {
    return loader->getInputBuffers();
}

std::vector<DeviceBuffer> HostParsedInference::getOutputBuffers() const {
    return loader->getOutputBuffers();
}

std::vector<DeviceBuffer> HostParsedInference::getProfBuffers() const {
    return loader->getProfBuffers();
}

std::shared_ptr<const elf::NetworkMetadata> HostParsedInference::getMetadata() {
    return loader->getNetworkMetadata();
}

void HostParsedInference::applyInputOutput(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs,
                                           std::vector<DeviceBuffer>& profiling) {
    return loader->applyJitRelocations(inputs, outputs, profiling);
}

}  // namespace elf
