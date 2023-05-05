// clang-format off
#include <vpux_loader/vpux_hpi.hpp>
#include <api/vpu_nnrt_api.h>
#include <api/vpu_nce_hw_mtl.h>
#include <vpux_sym_tab/3720/SymTabGen.h>
#include <vpux_elf/reader.hpp>
// clang-format on

namespace elf {

namespace {
// Base of frequency values used in tables (in MHz).
static constexpr uint32_t FREQ_BASE = 700;
// Step of frequency for each entry in tables (in MHz).
static constexpr uint32_t FREQ_STEP = 100;
// Base of bandwidth values used in tables (in MB/s).
static constexpr uint32_t BW_BASE = 2000;
// Step of bandwidth values used in tables (in MB/s).
static constexpr uint32_t BW_STEP = 100;

// value in [0.0..1.0] range indicating scalability of network for a given DDR bandwidth.
static const std::array<float, nn_public::VPU_SCALABILITY_VALUES_PER_FREQ> byBWScales({0.0F, 0.2F, 0.4F, 0.6F, 0.8F});
// expected ticks (based on FRC @37.5MHz) an inference should take for a given DDR bandwidth.
static const std::array<uint64_t, nn_public::VPU_SCALABILITY_VALUES_PER_FREQ> byBWTicks({10UL, 12UL, 14UL, 16UL, 18UL});

} // namespace

static void setDefaultPerformanceMetrics(nn_public::VpuPerformanceMetrics &metrics) {
    metrics.bw_base = BW_BASE;
    metrics.bw_step = BW_STEP;
    metrics.freq_base = FREQ_BASE;
    metrics.freq_step = FREQ_STEP;

    for (uint32_t i = 0; i < nn_public::VPU_SCALABILITY_NUM_OF_FREQ; ++i) {
        std::copy(byBWScales.begin(), byBWScales.end(), std::begin(metrics.scalability[i]));
        std::copy(byBWTicks.begin(), byBWTicks.end(), std::begin(metrics.ticks[i]));
    }
}

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
            auto metadata = *section.getData<NetworkMetadata>();
            return metadata.resource_requirements;
        }
    }

    VPUX_ELF_THROW(HeaderError, "Failed to find a resource");
}

HostParsedInference::HostParsedInference(BufferManager* bufferMgr, AccessManager* accessMgr): bufferManager(bufferMgr) {
    // EISW-73555
    // For now, only generate 1 mapped inference, even if the network is compiled for a single tile
    resRequirements = readResourcesFromElf(accessMgr);

    loaders.push_back(std::make_unique<VPUXLoader>(accessMgr, bufferManager,
                                                   SymTabGen::getSymTab(resRequirements.nn_slice_count_)));

    parsedInference = bufferManager->allocate(
            BufferSpecs(DEFAULT_ALIGN, sizeof(nn_public::VpuHostParsedInference), SHF_EXECINSTR));

    auto hpi = reinterpret_cast<nn_public::VpuHostParsedInference*>(parsedInference.cpu_addr());
    hpi->resource_requirements_ = {};
    hpi->resource_requirements_.nn_slice_count_ = resRequirements.nn_slice_count_;
    hpi->resource_requirements_.nn_barriers_ = resRequirements.nn_barriers_;

    setDefaultPerformanceMetrics(hpi->performance_metrics_);

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

ArrayRef<DeviceBuffer> HostParsedInference::getProfBuffers() const {
    return getBuffers(loaders, profiling, std::mem_fn(&VPUXLoader::getProfBuffers));
}

NetworkMetadata HostParsedInference::getMetadata() {
    return loaders.front()->getNetworkMetadata();
}

void HostParsedInference::applyInputOutput(std::vector<DeviceBuffer>& inputs, std::vector<DeviceBuffer>& outputs, std::vector<DeviceBuffer>& profiling) {
    for (auto const& loader : loaders) {
        return loader->applyJitRelocations(inputs, outputs, profiling);
    }
}

}  // namespace elf
