
//
// Copyright (C) 2025 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

// clang-format off
#include <hpi_5000.hpp>

// clang-format on

namespace elf {

namespace {

#ifdef INTEL_EMBARGO_NPU5
constexpr uint32_t VPUX50XX_VERSION_MAJOR = 2;
constexpr uint32_t VPUX50XX_VERSION_MINOR = 1;
constexpr uint32_t VPUX50XX_VERSION_PATCH = 0;

// 2.1.0
// - Add support for DMA symbol section for dynamic strides
//
// 2.0.0:
// - Bump ELF major version to reject all pre-PV NPU5 blobs
//
// 1.2.7:
// - Add support for elf::OVNodeType::I2
// - Add support for elf::OVNodeType::U2
//
// 1.2.6:
// - Add support for elf::DType::F8E8M0
// - Rename elf::DType::FP8 -> elf::DType::F8EM5M2
// - Rename elf::DType::HF8 -> elf::DType::F8E4M3FN

#endif  // INTEL_EMBARGO_NPU5

#ifdef INTEL_EMBARGO_NPU6
constexpr uint32_t VPUX60XX_VERSION_MAJOR = 1;
constexpr uint32_t VPUX60XX_VERSION_MINOR = 3;
constexpr uint32_t VPUX60XX_VERSION_PATCH = 2;

// Patch version 1: Adding FP8 data types

#endif  // INTEL_EMBARGO_NPU6

#ifdef INTEL_EMBARGO_NPU7
constexpr uint32_t VPUX70XX_VERSION_MAJOR = 1;
constexpr uint32_t VPUX70XX_VERSION_MINOR = 3;
constexpr uint32_t VPUX70XX_VERSION_PATCH = 2;

// Patch version 2: Initial support for NPU7

#endif  // INTEL_EMBARGO_NPU7

}  // namespace

// By building base HostParsedInference_4000 with default ctor we ensure special CMX symtabs are initialized empty
HostParsedInference_5000::HostParsedInference_5000(elf::platform::ArchKind archKind): HostParsedInference_4000_Base() {
    archKind_ = archKind;
}

elf::Version HostParsedInference_5000::getELFLibABIVersion() const {
    switch (archKind_) {
#ifdef INTEL_EMBARGO_NPU5
    case elf::platform::ArchKind::VPUX501X:
    case elf::platform::ArchKind::VPUX502X:
        return {VPUX50XX_VERSION_MAJOR, VPUX50XX_VERSION_MINOR, VPUX50XX_VERSION_PATCH};
#endif  // INTEL_EMBARGO_NPU5
#ifdef INTEL_EMBARGO_NPU6
    case elf::platform::ArchKind::VPUX60XX:
        return {VPUX60XX_VERSION_MAJOR, VPUX60XX_VERSION_MINOR, VPUX60XX_VERSION_PATCH};
#endif  // INTEL_EMBARGO_NPU6
#ifdef INTEL_EMBARGO_NPU7
    case elf::platform::ArchKind::VPUX70XX:
        return {VPUX70XX_VERSION_MAJOR, VPUX70XX_VERSION_MINOR, VPUX70XX_VERSION_PATCH};
#endif  // INTEL_EMBARGO_NPU7
    default:
        break;
    }
    VPUX_ELF_THROW(RangeError, (elf::platform::stringifyArchKind(archKind_) + " arch is not supported").c_str());
    return {0, 0, 0};
}

}  // namespace elf
