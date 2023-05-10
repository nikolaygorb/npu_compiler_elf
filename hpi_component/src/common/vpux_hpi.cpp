#ifndef VPUX_ELF_LOG_UNIT_NAME
#define VPUX_ELF_LOG_UNIT_NAME "VpuxHpi"
#endif
// clang-format off
#include <vpux_elf/utils/log.hpp>
#include <vpux_elf/reader.hpp>
#include <vpux_hpi.hpp>

#if defined(CONFIG_TARGET_SOC_3720) || defined(HOST_BUILD)
#include <hpi_3720.hpp>
#endif

#ifdef CONFIG_TARGET_SOC_4000 // EISW-77741
#include <hpi_4000.hpp>
#include <MvStringSecure.h>
#endif

#include <string.h>
// clang-format on

namespace elf {

enum ArchKind {
    UNKNOWN = 0,
    VPUX37XX,
    VPUX40XX

};

namespace {
template <typename T>
static ArrayRef<DeviceBuffer> getBuffers(const std::unique_ptr<VPUXLoader> &loader, std::vector<DeviceBuffer> &vec,
                                         T &&get) {
    // Clear vector to ensure we are always in sync with latest allocation state from loader
    vec.clear();

    auto buffs = get(loader);
    std::copy(buffs.begin(), buffs.end(), std::back_inserter(vec));

    return ArrayRef<DeviceBuffer>(vec);
}

static ResourceRequirements readResourcesFromElf(AccessManager *elfAccess) {
    /* TODO: Temporary solution copied from InferenceManagerDemo */
    // Temporary solution:
    // The loader must be initialized with a pre-generated symtab.
    // To generate a symtab for a configuration (one cluster/two clusters), the resource
    // requirements shoud be read before the loader starts to apply relocations.
    // Issue should be addressed with EISW-73309

    Reader<ELF_Bitness::Elf64> reader(elfAccess);

    auto nSections = reader.getSectionsNum();

    for (size_t i = 0; i < nSections; i++) {
        const auto &section = reader.getSection(i);

        const auto sectionHeader = section.getHeader();
        auto sectionType = sectionHeader->sh_type;

        if (sectionType == elf::VPU_SHT_NETDESC) {
            ResourceRequirements res{};
            memcpy(&res, &section.getData<NetworkMetadata>()->resource_requirements, sizeof(res));
            return res;
        }
    }

#ifdef CONFIG_TARGET_SOC_4000
    ResourceRequirements res{};
    // TODO: At this point we are not able to test
    // the functionality with a proper generated ELF
    // EISW-79514
    res.nn_slice_count_ = 1;
    res.nn_barriers_ = 16;
    return res;
#endif

    VPUX_ELF_THROW(HeaderError, "Failed to find a resource");
}

const static std::unordered_map<std::string, ArchKind> knownArch = {{"VPUX37XX", ArchKind::VPUX37XX},
                                                                    {"VPUX40XX", ArchKind::VPUX40XX}};

static ArchKind readArchKind(AccessManager *elfAccess) {
    Reader<ELF_Bitness::Elf64> reader(elfAccess);

    auto nSections = reader.getSectionsNum();

    for (size_t i = 0; i < nSections; i++) {
        const auto &section = reader.getSection(i);

        const auto sectionHeader = section.getHeader();
        auto sectionType = sectionHeader->sh_type;

        if (sectionType == elf::VPU_SHT_NETDESC) {
            char archName[MAX_STRING_LEN];
            strncpy(archName, section.getData<NetworkMetadata>()->arch_name, MAX_STRING_LEN);
            return knownArch.find(archName)->second;
        }
    }

// TODO: At this point we are not able to test
// the functionality with a proper generated ELF
// EISW-79514
#ifdef CONFIG_TARGET_SOC_4000
    return knownArch.find("VPUX40XX")->second;
#endif

    return ArchKind::UNKNOWN;
}

} // namespace

HostParsedInference::HostParsedInference(BufferManager *bufferMgr, AccessManager *accessMgr)
    : bufferManager(bufferMgr) {
    ArchKind arch = readArchKind(accessMgr);
    resRequirements = readResourcesFromElf(accessMgr);

    VPUX_ELF_LOG(LogLevel::DEBUG, "Creating specialized HPI for arch %u", arch);

    // TODO: EISW-79344
    std::unique_ptr<HostParsedInferenceCommon> obj;
    switch (arch) {
#if defined(CONFIG_TARGET_SOC_3720) || defined(HOST_BUILD)
        case ArchKind::VPUX37XX:
            obj.reset(new HostParsedInference_3720());
            break;
#endif
#ifdef CONFIG_TARGET_SOC_4000 // EISW-77741
        case ArchKind::VPUX40XX:
            obj.reset(new HostParsedInference_4000());
            break;
#endif
        default:
            VPUX_ELF_THROW(RangeError, "Arch not in range");
            break;
    }

    VPUX_ELF_THROW_WHEN(obj.get() == nullptr, AllocError, "Allocation Error!");

    // EISW-73555
    loader = std::make_unique<VPUXLoader>(accessMgr, bufferManager, obj->getSymTab(resRequirements.nn_slice_count_));

    // DeviceBuffer getting a pointer to arch specific host parsed inference
    parsedInference = obj->allocHostParsedInference(bufferManager);

    // reinterpret cast the device buffer to the arch specific strucutres
    // and set the fields required for the executions of the mapped inference
    obj->setHostParsedInference(parsedInference, loader->getEntry(), resRequirements);
}

HostParsedInference::~HostParsedInference() {
    bufferManager->deallocate(parsedInference);
}

DeviceBuffer HostParsedInference::getParsedInference() {
    return parsedInference;
}

ArrayRef<DeviceBuffer> HostParsedInference::getAllocatedBuffers() const {
    return getBuffers(loader, allocations, std::mem_fn(&VPUXLoader::getAllocatedBuffers));
}

ArrayRef<DeviceBuffer> HostParsedInference::getInputBuffers() const {
    return getBuffers(loader, inputs, std::mem_fn(&VPUXLoader::getInputBuffers));
}

ArrayRef<DeviceBuffer> HostParsedInference::getOutputBuffers() const {
    return getBuffers(loader, outputs, std::mem_fn(&VPUXLoader::getOutputBuffers));
}

ArrayRef<DeviceBuffer> HostParsedInference::getProfBuffers() const {
    return getBuffers(loader, profiling, std::mem_fn(&VPUXLoader::getProfBuffers));
}

NetworkMetadata HostParsedInference::getMetadata() {
    return loader->getNetworkMetadata();
}

void HostParsedInference::applyInputOutput(std::vector<DeviceBuffer> &inputs, std::vector<DeviceBuffer> &outputs,
                                           std::vector<DeviceBuffer> &profiling) {
    return loader->applyJitRelocations(inputs, outputs, profiling);
}

} // namespace elf
