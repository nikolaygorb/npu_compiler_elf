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
namespace {

const static std::unordered_map<std::string,
                        HostParsedInference::ArchKind> knownArch = {{"UNKNOWN", HostParsedInference::ArchKind::UNKNOWN},
                                                                    {"VPUX37XX", HostParsedInference::ArchKind::VPUX37XX},
                                                                    {"VPUX40XX", HostParsedInference::ArchKind::VPUX40XX},
                                                                    {"VPUX50XX", HostParsedInference::ArchKind::VPUX50XX}};

static HostParsedInference::ArchKind mapArchStringToArchKind(const std::string& archName) {
    auto retArch = knownArch.find(archName);
    if (retArch != knownArch.end()) {
        return retArch->second;
    } else {
        return HostParsedInference::ArchKind::UNKNOWN;
    }
}

static std::string archKindToString(HostParsedInference::ArchKind arch)
{
    for(auto archIt : knownArch)
    {
        if(archIt.second == arch)
        {
            return archIt.first;
        }
    }
    return std::string("UNKNOWN");
}

static std::unique_ptr<HostParsedInferenceCommon> getArchSpecificHPI(const std::string& archName) {
    auto arch = mapArchStringToArchKind(archName);

    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "Creating specialized HPI for arch %u", arch);

    std::unique_ptr<HostParsedInferenceCommon> archSpecificHPI;
    switch (arch) {
#if defined(CONFIG_TARGET_SOC_3720) || defined(HOST_BUILD)
    case HostParsedInference::ArchKind::VPUX37XX:
        archSpecificHPI = std::make_unique<HostParsedInference_3720>();
        break;
#endif

// to be updated with E#88139:
// temporary fix to support NPU 5000 arch
#if defined(CONFIG_TARGET_SOC_4000) || (CONFIG_TARGET_SOC_5000) || defined(HOST_BUILD)
    case HostParsedInference::ArchKind::VPUX40XX:
    case HostParsedInference::ArchKind::VPUX50XX:
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

const elf::Version HostParsedInference::readVersioningInfo(uint32_t versionType) const {
    const auto& noteSections = loader->getSectionsOfType(elf::SHT_NOTE);
    for (auto section : noteSections) {
        VPUX_ELF_THROW_UNLESS(section.size() == sizeof(elf::elf_note::VersionNote), SectionError, "Wrong Versioning Note size");
        elf::elf_note::VersionNote elfABIVersionNote{};
        memcpy(&elfABIVersionNote, section.cpu_addr(), sizeof(elf::elf_note::VersionNote));
        if (elfABIVersionNote.n_type == versionType) {
            return elf::Version(elfABIVersionNote);
        }
    }
    VPUX_ELF_LOG(LogLevel::LOG_ERROR, "Could not retrieve versioning info of type %x", versionType);
    VPUX_ELF_THROW(RangeError, "Requested Versioning information was not found");
}

const elf::Version HostParsedInference::getElfABIVersion() const {
    return readVersioningInfo(elf::elf_note::NT_GNU_ABI_TAG);
}

const elf::Version HostParsedInference::getMIVersion() const {
    return readVersioningInfo(elf::elf_note::NT_NPU_MPI_VERSION);
}

HostParsedInference::HostParsedInference(BufferManager* bufferMgr, AccessManager* accessMgr, ArchKind expArchKind)
        : bufferManager(bufferMgr), accessManager(accessMgr) {
    // create the loader object to cache sections
    loader = std::make_unique<VPUXLoader>(accessMgr, bufferMgr);
    // read metadata in order to have access arch name
    readMetadata();
    auto archName = std::string(metadata->mIdentification.arch_name);
    auto expArchName = archKindToString(expArchKind);
    VPUX_ELF_THROW_WHEN(mapArchStringToArchKind(archName) != expArchKind, ArgsError, "Expected arch %s but receieved %s.", archName, expArchName);
}

void HostParsedInference::load() {
    auto archName = std::string(metadata->mIdentification.arch_name);

    // TODO: E#79344
    auto archSpecificHpi = getArchSpecificHPI(archName);

    // Check ELF Library ABI Compatibility
    elf::Version::checkVersionCompatibility(archSpecificHpi->getELFLibABIVersion(), getElfABIVersion(), elf::VersionType::ELF_ABI_VERSION);

    // Check Mapped Inference Compatibility
    elf::Version::checkVersionCompatibility(archSpecificHpi->getExpectedMIVersion(), getMIVersion(), elf::VersionType::MAPPED_INFERENCE_VERSION);

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
    return metadata;
}

void HostParsedInference::applyInputOutput(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs,
                                           std::vector<DeviceBuffer>& profiling) {
    return loader->applyJitRelocations(inputs, outputs, profiling);
}

}  // namespace elf
