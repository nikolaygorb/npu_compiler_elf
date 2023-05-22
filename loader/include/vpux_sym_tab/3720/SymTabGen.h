//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

// clang-format off
#include <vpux_loader/vpux_loader.hpp>
#include <api/vpu_nce_hw_37xx.h>
//clang-format on

namespace elf {

class SymTabGen {
public:
    SymTabGen(const SymTabGen& other) = delete;
    void operator=(const SymTabGen& rhs) = delete;
    static ArrayRef<SymbolEntry> getSymTab(uint8_t index);

private:
    static constexpr uint8_t N_TABS = nn_public::VPU_MAX_TILES;
    static constexpr size_t SPECIAL_SYMTAB_SIZE = 8;
    static SymbolEntry symTab_[N_TABS][SPECIAL_SYMTAB_SIZE];

    SymTabGen();
    static void initSymTab();
};

}  // namespace elf
