// clang-format off
#include <vpux_loader/vpux_hpi.hpp>
#include <api/vpu_nnrt_api.h>
#include <api/vpu_nce_hw_mtl.h>
#include <vpux_sym_tab/3720/SymTabGen.h>
#include <vpux_elf/reader.hpp>
// clang-format on

namespace elf {

constexpr auto DEFAULT_ALIGN = 64;

template <typename T>
static ArrayRef<DeviceBuffer> getBuffers(const std::vector<std::unique_ptr<VPUXLoader>>& loaders,
                                         std::vector<DeviceBuffer>& vec, T&& get) {
    // Clear vector to ensure we are always in sync with latest allocation state from loaders
    vec.clear();

    for (auto const& loader : loaders) {
        auto buffs = get(loader);
        std::copy(buffs.begin(), buffs.end(), std::back_inserter(vec));
    }
    return ArrayRef<DeviceBuffer>(vec);
}

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
            return *(section.getData<ResourceRequirements>());
        }
    }

    throw std::runtime_error("Failed to find a resource");
}

HostParsedInference::HostParsedInference(BufferManager* bufferMgr, AccessManager* accessMgr): bufferManager(bufferMgr) {
    resRequirements = readResourcesFromElf(accessMgr);

    // EISW-73555
    // For now, only generate 1 mapped inference, even if the network is compiled for a single tile
    try {
        loaders.push_back(std::make_unique<VPUXLoader>(accessMgr, bufferManager,
                                                       SymTabGen::getSymTab(resRequirements.nn_slice_count_)));
    } catch (...) {
        // Rethrow to let driver know that construction failed and HostParsedInference object cannot be further used
        throw;
    }

    parsedInference = bufferManager->allocate(
            BufferSpecs(DEFAULT_ALIGN, sizeof(nn_public::VpuHostParsedInference), SHF_EXECINSTR));

    auto hpi = reinterpret_cast<nn_public::VpuHostParsedInference*>(parsedInference.cpu_addr());
    hpi->resource_requirements_ = {};
    hpi->resource_requirements_.nn_slice_count_ = resRequirements.nn_slice_count_;
    hpi->resource_requirements_.nn_barriers_ = resRequirements.nn_barriers_;
    hpi->performance_metrics_ = {};

    hpi->mapped_.address = loaders.front()->getEntry();
    hpi->mapped_.count = 1;
}

HostParsedInference::~HostParsedInference() {
    bufferManager->deallocate(parsedInference);
}

DeviceBuffer HostParsedInference::getParsedInference() {
    return parsedInference;
}

ArrayRef<DeviceBuffer> HostParsedInference::getAllocatedBuffers() const {
    return getBuffers(loaders, allocations, std::mem_fn(&VPUXLoader::getAllocatedBuffers));
}

ArrayRef<DeviceBuffer> HostParsedInference::getInputBuffers() const {
    return getBuffers(loaders, inputs, std::mem_fn(&VPUXLoader::getInputBuffers));
}

ArrayRef<DeviceBuffer> HostParsedInference::getOutputBuffers() const {
    return getBuffers(loaders, outputs, std::mem_fn(&VPUXLoader::getOutputBuffers));
}

NetworkMetadata HostParsedInference::getMetadata() {
    return loaders.front()->getNetworkMetadata();
}

void HostParsedInference::applyInputOutput(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs) {
    for (auto const& loader : loaders) {
        return loader->applyJitRelocations(inputs, outputs);
    }
}

}  // namespace elf
