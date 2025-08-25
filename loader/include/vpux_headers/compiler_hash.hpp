//
// Copyright (C) 2025 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <vpux_headers/metadata_primitives.hpp>

namespace elf {

// Current just store 40 bytes hash string
constexpr uint8_t compilerHashStringLen = 40;

struct VPUX_ALIGNED_STRUCT(8) CompilerHashInfo {
    uint8_t mCompilerHash[compilerHashStringLen];
};

class CompilerHash final {
public:
    CompilerHash() = delete;
    explicit CompilerHash(const elf::CompilerHashInfo& compilerHashInfo) : isValid(true) {
        std::memcpy(mCompilerHash, compilerHashInfo.mCompilerHash, compilerHashStringLen);
    }

    const std::string getCompilerHash() const {
        return std::string(reinterpret_cast<const char*>(mCompilerHash), compilerHashStringLen);
    }

private:
    bool isValid = false;
    uint8_t mCompilerHash[compilerHashStringLen];
};


}  // namespace elf
