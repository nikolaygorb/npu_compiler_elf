//
// Copyright (C) 2023-2025 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//
#if defined(CONFIG_TARGET_SOC_3720) || defined(HOST_BUILD)
#include <hpi_3720.hpp>
#endif

#if defined(CONFIG_TARGET_SOC_4000) || defined(HOST_BUILD)
#include <hpi_4000.hpp>
#endif
#if (defined(CONFIG_TARGET_SOC_5000) || \
    (defined(CONFIG_TARGET_SOC_6000) || \
     defined(CONFIG_TARGET_SOC_7000) || defined(HOST_BUILD)))
#include <hpi_5000.hpp>
#endif
#include <hpi_common_interface.hpp>

namespace elf {

// Default implementations will be overriden as needed by derived classes

std::unique_ptr<HostParsedInferenceCommon> HostParsedInferenceCommon::getArchSpecificHPI(elf::platform::ArchKind archKind) {
    VPUX_ELF_LOG(LogLevel::LOG_DEBUG, "Creating specialized HPI for arch %u", archKind);

    std::unique_ptr<HostParsedInferenceCommon> archSpecificHPI;
    switch (archKind) {
#if defined(CONFIG_TARGET_SOC_3720) || defined(HOST_BUILD)
    case elf::platform::ArchKind::VPUX37XX:
        archSpecificHPI = std::make_unique<HostParsedInference_3720>();
        break;
#endif

#if defined(CONFIG_TARGET_SOC_4000) || defined(HOST_BUILD)
    case elf::platform::ArchKind::VPUX40XX:
        archSpecificHPI = std::make_unique<HostParsedInference_4000>(archKind);
        break;
#endif
#if (defined(CONFIG_TARGET_SOC_5000) || defined(HOST_BUILD))
    case elf::platform::ArchKind::VPUX501X:
    case elf::platform::ArchKind::VPUX502X:
        archSpecificHPI = std::make_unique<HostParsedInference_5000>(archKind);
        break;
#endif
#if (defined(CONFIG_TARGET_SOC_6000) || defined(HOST_BUILD)) && defined(INTEL_EMBARGO_NPU6)
        // to be updated with E#88139: temporary fix to support NPU 6000 arch
    case elf::platform::ArchKind::VPUX60XX:
        archSpecificHPI = std::make_unique<HostParsedInference_5000>(archKind);
        break;
#endif  // INTEL_EMBARGO_NPU6
#if (defined(CONFIG_TARGET_SOC_7000) || defined(HOST_BUILD)) && defined(INTEL_EMBARGO_NPU7)
        // to be updated with E#88139: temporary fix to support NPU 7000 arch
    case elf::platform::ArchKind::VPUX70XX:
        archSpecificHPI = std::make_unique<HostParsedInference_5000>(archKind);
        break;
#endif  // INTEL_EMBARGO_NPU7
    default:
        VPUX_ELF_THROW(RangeError, (elf::platform::stringifyArchKind(archKind) + " arch is not supported").c_str());
        break;
    }

    return archSpecificHPI;
}

std::vector<elf::Elf_Word> HostParsedInferenceCommon::getSymbolSectionTypes() const {
    return {};
}

bool HostParsedInferenceCommon::getExplicitAllocationsEnabled() const {
    return false;
}

BufferSpecs HostParsedInferenceCommon::getEntryBufferSpecs(size_t numOfEntries) {
    (void)numOfEntries;
    return {};
}

}  // namespace elf
