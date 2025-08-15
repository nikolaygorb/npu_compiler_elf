//
// Copyright (C) 2025 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <vpux_headers/serial_struct_base.hpp>

namespace elf {

struct CompilerHashInfo {
    std::vector<char> mCompilerHash;
};

class SerialCompilerHashInfo : public elf::SerialStructBase {
public:
    SerialCompilerHashInfo(CompilerHashInfo& compilerHashInfo) {
        addElementVector(compilerHashInfo.mCompilerHash);
    }
};

class CompilerHashInfoSerialization {
public:
    static std::vector<uint8_t> serialize(CompilerHashInfo& compilerHashInfo) {
        return SerialCompilerHashInfo(compilerHashInfo).serialize();
    }

    static const std::shared_ptr<CompilerHashInfo> deserialize(const uint8_t* buffer, uint64_t size) {
        const auto compilerHashInfo = std::make_shared<CompilerHashInfo>();
        SerialCompilerHashInfo(*compilerHashInfo).deserialize(buffer, size);

        return compilerHashInfo;
    }
};

}  // namespace elf
