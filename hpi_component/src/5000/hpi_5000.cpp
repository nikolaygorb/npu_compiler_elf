
//
// Copyright (C) 2025-2026 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

// clang-format off
#include <hpi_5000.hpp>

// clang-format on

namespace elf {

namespace {

constexpr uint32_t VPUX50XX_VERSION_MAJOR = 2;
constexpr uint32_t VPUX50XX_VERSION_MINOR = 2;
constexpr uint32_t VPUX50XX_VERSION_PATCH = 5;

// 2.2.5
// - allow normalized 0 alignment
//
// 2.2.4
// - Fix DMA JIT user-stride copy size to avoid out-of-bounds read
//
// 2.2.3
// - Add support for VPU_SHT_COMPATIBILITY_STRING section type
//
// 2.2.2
// - Fix header offset overflow in Reader when section table offset is close to file size limit
//
// 2.2.1
// - Fix security vulnerabilities
//
// 2.2.0
// - Enable direct MMI support
//
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

#ifdef INTEL_EMBARGO_NPU6
constexpr uint32_t VPUX60XX_VERSION_MAJOR = 1;
constexpr uint32_t VPUX60XX_VERSION_MINOR = 5;
constexpr uint32_t VPUX60XX_VERSION_PATCH = 5;

// 1.5.5
// - allow normalized 0 alignment
//
// 1.5.4
// - Fix DMA JIT user-stride copy size to avoid out-of-bounds read
//
// 1.5.3
// - Add support for VPU_SHT_COMPATIBILITY_STRING section type
//
// 1.5.2
// - Fix header offset overflow in Reader when section table offset is close to file size limit
//
// 1.5.1
// - Fix security vulnerabilities
//
// 1.5.0
// - Enable direct MMI support
//
// 1.4.0
// - Add support for DMA symbol section for dynamic strides
//
// Patch version 1: Adding FP8 data types

#endif  // INTEL_EMBARGO_NPU6

#ifdef INTEL_EMBARGO_NPU7
constexpr uint32_t VPUX70XX_VERSION_MAJOR = 1;
constexpr uint32_t VPUX70XX_VERSION_MINOR = 5;
constexpr uint32_t VPUX70XX_VERSION_PATCH = 5;

// 1.5.5
// - allow normalized 0 alignment
//
// 1.5.4
// - Fix DMA JIT user-stride copy size to avoid out-of-bounds read
//
// 1.5.3
// - Add support for VPU_SHT_COMPATIBILITY_STRING section type
//
// 1.5.2
// - Fix header offset overflow in Reader when section table offset is close to file size limit
//
// 1.5.1
//  - Fix security vulnerabilities
//
// 1.5.0
// - Enable direct MMI support
//
// 1.4.0
// - Add support for DMA symbol section for dynamic strides
//
// Patch version 2: Initial support for NPU7

#endif  // INTEL_EMBARGO_NPU7

#ifdef INTEL_EMBARGO_NPU8
constexpr uint32_t VPUX80XX_VERSION_MAJOR = 1;
constexpr uint32_t VPUX80XX_VERSION_MINOR = 5;
constexpr uint32_t VPUX80XX_VERSION_PATCH = 5;

// 1.5.5
// - allow normalized 0 alignment
//
// 1.5.4
// - Fix DMA JIT user-stride copy size to avoid out-of-bounds read
//
// 1.5.3
// - Add support for VPU_SHT_COMPATIBILITY_STRING section type
//
// 1.5.2
// - Fix header offset overflow in Reader when section table offset is close to file size limit
//
// 1.5.1
// - Fix security vulnerabilities
//
// 1.5.0
// - Initial support for NPU8

#endif  // INTEL_EMBARGO_NPU8

}  // namespace

// By building base HostParsedInference_4000 with default ctor we ensure special CMX symtabs are initialized empty
HostParsedInference_5000::HostParsedInference_5000(elf::platform::ArchKind archKind): HostParsedInference_4000_Base() {
    archKind_ = archKind;
}

elf::Version HostParsedInference_5000::getELFLibABIVersion() const {
    switch (archKind_) {
    case elf::platform::ArchKind::VPUX501X:
    case elf::platform::ArchKind::VPUX502X:
        return {VPUX50XX_VERSION_MAJOR, VPUX50XX_VERSION_MINOR, VPUX50XX_VERSION_PATCH};
#ifdef INTEL_EMBARGO_NPU6
    case elf::platform::ArchKind::VPUX60XX:
        return {VPUX60XX_VERSION_MAJOR, VPUX60XX_VERSION_MINOR, VPUX60XX_VERSION_PATCH};
#endif  // INTEL_EMBARGO_NPU6
#ifdef INTEL_EMBARGO_NPU7
    case elf::platform::ArchKind::VPUX70XX:
        return {VPUX70XX_VERSION_MAJOR, VPUX70XX_VERSION_MINOR, VPUX70XX_VERSION_PATCH};
#endif  // INTEL_EMBARGO_NPU7
#ifdef INTEL_EMBARGO_NPU8
    case elf::platform::ArchKind::VPUX80XX:
        return {VPUX80XX_VERSION_MAJOR, VPUX80XX_VERSION_MINOR, VPUX80XX_VERSION_PATCH};
#endif  // INTEL_EMBARGO_NPU8
    default:
        break;
    }
    VPUX_ELF_THROW(RangeError, (elf::platform::stringifyArchKind(archKind_) + " arch is not supported").c_str());
    return {0, 0, 0};
}

}  // namespace elf
