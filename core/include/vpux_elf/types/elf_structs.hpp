//
// Copyright Intel Corporation.
//
// LEGAL NOTICE: Your use of this software and any required dependent software
// (the "Software Package") is subject to the terms and conditions of
// the Intel(R) OpenVINO(TM) Distribution License for the Software Package,
// which may also include notices, disclaimers, or license terms for
// third party or open source software included in or with the Software Package,
// and your use indicates your acceptance of all such terms. Please refer
// to the "third-party-programs.txt" or other similarly-named text file
// included with the Software Package for additional details.
//

#pragma once

#include <vpux_elf/types/data_types.hpp>
#include <vpux_elf/types/elf_header.hpp>
#include <vpux_elf/types/program_header.hpp>
#include <vpux_elf/types/relocation_entry.hpp>
#include <vpux_elf/types/section_header.hpp>
#include <vpux_elf/types/symbol_entry.hpp>


namespace elf {

enum ELF_Bitness {
    Elf32 = 32,
    Elf64 = 64
};

template <ELF_Bitness B>
struct ElfTypes {
};

template <>
struct ElfTypes<ELF_Bitness::Elf32> {
    using ELFHeader = Elf32_Ehdr;
    using ProgramHeader = Elf32_Phdr;
    using RelocationEntry = Elf32_Rel;
    using RelocationAEntry = Elf32_Rela;
    using SectionHeader = Elf32_Shdr;
    using SymbolEntry = Elf32_Sym;
};

template <>
struct ElfTypes<ELF_Bitness::Elf64> {
    using ELFHeader = Elf64_Ehdr;
    using ProgramHeader = Elf64_Phdr;
    using RelocationEntry = Elf64_Rel;
    using RelocationAEntry = Elf64_Rela;
    using SectionHeader = Elf64_Shdr;
    using SymbolEntry = Elf64_Sym;
};


} // namespace elf
