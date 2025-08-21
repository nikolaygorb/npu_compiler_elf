//
// Copyright (C) 2025 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <vpux_headers/serial_struct_base.hpp>
#include <vpux_headers/metadata_primitives.hpp>

namespace elf {

struct VPUX_ALIGNED_STRUCT(8) CompilerHashInfo {
    std::vector<char> mCompilerHash;
};

class SerialCompilerHashInfo : public elf::SerialStructBase {
public:
    SerialCompilerHashInfo(CompilerHashInfo& compilerHashInfo) {
        addElementVector(compilerHashInfo.mCompilerHash);
    }
};

using CompilerHashInfoSerialization = SerialAccess<CompilerHashInfo, SerialCompilerHashInfo>;

}  // namespace elf
