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
#include <vpux_elf/reader.hpp>
#include <vpux_hpi.hpp>

#if defined(CONFIG_TARGET_SOC_3720) || defined(HOST_BUILD)
#include <hpi_3720.hpp>
#endif

#if defined(CONFIG_TARGET_SOC_4000) || defined(HOST_BUILD)
#include <hpi_4000.hpp>
#endif

//to be removed with EISW-88139:
//temporary fix to support VPU5 arch
#if defined(CONFIG_TARGET_SOC_5000) || defined(HOST_BUILD)
#include <hpi_4000.hpp>
#endif

#include <string.h>
// clang-format on

namespace elf {

enum ArchKind { UNKNOWN = 0, VPUX37XX, VPUX40XX };

namespace {

static ResourceRequirements readResourcesFromElf(AccessManager* elfAccess) {
    /* TODO: Temporary solution copied from InferenceManagerDemo */
    // Temporary solution:
    // The loader must be initialized with a pre-generated symtab.
    // To generate a symtab for a configuration (one cluster/two clusters), the resource
    // requirements shoud be read before the loader starts to apply relocations.
    // Issue should be addressed with EISW-73309

    Reader<ELF_Bitness::Elf64> reader(elfAccess);

    auto nSections = reader.getSectionsNum();

    for (size_t i = 0; i < nSections; i++) {
        const auto& section = reader.getSection(i);

        const auto sectionHeader = section.getHeader();
        auto sectionType = sectionHeader->sh_type;

        if (sectionType == elf::VPU_SHT_NETDESC) {
            ResourceRequirements res{};
            memcpy(&res, &section.getData<NetworkMetadata>()->resource_requirements, sizeof(res));
            return res;
        }
    }

    VPUX_ELF_THROW(HeaderError, "Failed to find a resource");
}

const static std::unordered_map<std::string, ArchKind> knownArch = {{"VPUX37XX", ArchKind::VPUX37XX},
                                                                    {"VPUX40XX", ArchKind::VPUX40XX},
                                                                    //to be removed with EISW-88139:
                                                                    //temporary fix to support VPU5 arch
                                                                    {"VPUX50XX", ArchKind::VPUX40XX}};

static std::string readArchKind(AccessManager* elfAccess) {
    Reader<ELF_Bitness::Elf64> reader(elfAccess);

    auto nSections = reader.getSectionsNum();

    for (size_t i = 0; i < nSections; i++) {
        const auto& section = reader.getSection(i);

        const auto sectionHeader = section.getHeader();
        auto sectionType = sectionHeader->sh_type;

        if (sectionType == elf::VPU_SHT_NETDESC) {
            char archName[MAX_STRING_LEN] = {};
            strncpy(archName, section.getData<NetworkMetadata>()->arch_name, MAX_STRING_LEN);
            return std::string(archName);
        }
    }

    VPUX_ELF_THROW(RuntimeError, "Could not locate arch name");
    return std::string("UNKNOWN");
}

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

//to be updated with EISW-88139:
//temporary fix to support VPU5 arch
#if defined(CONFIG_TARGET_SOC_4000) || (CONFIG_TARGET_SOC_5000)|| defined(HOST_BUILD)
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

static std::unique_ptr<VPUXLoader> getLoader(BufferManager* bufferMgr, AccessManager* accessMgr,
                                             HostParsedInferenceCommon& hpiCommon,
                                             const ResourceRequirements& resRequirements) {
    // EISW-73555
    const auto symbolTable = hpiCommon.getSymbolTable(resRequirements.nn_slice_count_);
    const auto symbolSectionTypes = hpiCommon.getSymbolSectionTypes();
    auto symTabOverrideMode = hpiCommon.getSymbolSectionTypes().size() == 0 ? false : true;
    auto loader =
            std::make_unique<VPUXLoader>(accessMgr, bufferMgr, symbolTable, symTabOverrideMode, symbolSectionTypes);
    return loader;
}

}  // namespace

HostParsedInference::HostParsedInference(BufferManager* bufferMgr, AccessManager* accessMgr)
        : bufferManager(bufferMgr), accessManager(accessMgr) {
    auto archName = readArchKind(accessManager);
    resRequirements = readResourcesFromElf(accessManager);

    // TODO: EISW-79344
    auto archSpecificHpi = getArchSpecificHPI(archName);
    loader = getLoader(bufferManager, accessManager, *archSpecificHpi, resRequirements);
    parsedInference =
            std::make_shared<AllocatedDeviceBuffer>(bufferManager, archSpecificHpi->getParsedInferenceBufferSpecs());
    auto parsedInferenceBuffer = parsedInference->getBuffer();
    archSpecificHpi->setHostParsedInference(parsedInferenceBuffer, loader->getEntry(), resRequirements);
}

HostParsedInference::HostParsedInference(const HostParsedInference& other)
        : bufferManager(other.bufferManager),
          accessManager(other.accessManager),
          resRequirements(other.resRequirements) {
    auto archName = readArchKind(accessManager);

    // TODO: EISW-79344
    auto archSpecificHpi = getArchSpecificHPI(archName);
    // Use clone semantics here by copy-constructing the loader object
    loader = std::make_unique<VPUXLoader>(*other.loader);
    // Every new loader object means a new parsedInference struct as well
    parsedInference =
            std::make_shared<AllocatedDeviceBuffer>(bufferManager, archSpecificHpi->getParsedInferenceBufferSpecs());
    auto parsedInferenceBuffer = parsedInference->getBuffer();
    archSpecificHpi->setHostParsedInference(parsedInferenceBuffer, loader->getEntry(), resRequirements);
};

HostParsedInference::HostParsedInference(HostParsedInference&& other)
        : bufferManager(other.bufferManager),
          accessManager(other.accessManager),
          resRequirements(other.resRequirements),
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
    resRequirements = rhs.resRequirements;

    auto archName = readArchKind(accessManager);

    // TODO: EISW-79344
    auto archSpecificHpi = getArchSpecificHPI(archName);
    // Use clone semantics here by copy-constructing the loader object
    loader = std::make_unique<VPUXLoader>(*rhs.loader);
    // Every new loader object means a new parsedInference struct as well
    parsedInference =
            std::make_shared<AllocatedDeviceBuffer>(bufferManager, archSpecificHpi->getParsedInferenceBufferSpecs());
    auto parsedInferenceBuffer = parsedInference->getBuffer();
    archSpecificHpi->setHostParsedInference(parsedInferenceBuffer, loader->getEntry(), resRequirements);

    return *this;
}

HostParsedInference& HostParsedInference::operator=(HostParsedInference&& rhs) {
    if (this == &rhs) {
        return *this;
    }
    bufferManager = rhs.bufferManager;
    accessManager = rhs.accessManager;
    resRequirements = rhs.resRequirements;
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

NetworkMetadata HostParsedInference::getMetadata() {
    return loader->getNetworkMetadata();
}

void HostParsedInference::applyInputOutput(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs,
                                           std::vector<DeviceBuffer>& profiling) {
    return loader->applyJitRelocations(inputs, outputs, profiling);
}

}  // namespace elf
