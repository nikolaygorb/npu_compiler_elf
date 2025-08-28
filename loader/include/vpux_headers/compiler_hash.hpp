//
// Copyright (C) 2025 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <vpux_headers/serial_struct_base.hpp>

namespace elf {
struct CompilerHashInfo {
    std::vector<uint8_t> mCompilerHash;
};

class SerialCompielrInfo : public SerialStructBase {
public:
    SerialCompielrInfo(elf::CompilerHashInfo& compilerHashInfo) {
        addElementVector(compilerHashInfo.mCompilerHash);
    }
};

using CompilerHashSerialization = SerialAccess<CompilerHashInfo, SerialCompielrInfo>;

}  // namespace elf
