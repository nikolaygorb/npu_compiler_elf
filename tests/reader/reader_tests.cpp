//
// Copyright (C) 2025-2026 Intel Corporation.
// SPDX-License-Identifier: Apache-2.0
//

//

#include <cstring>

#include <vpux_elf/accessor.hpp>
#include <vpux_elf/reader.hpp>

#include <gtest/gtest.h>

using namespace elf;

// TODO: move to utils
#define OV_ASSERT_NO_THROW(statement) OV_ASSERT_NO_THROW_(statement, GTEST_FATAL_FAILURE_)
#define OV_ASSERT_NO_THROW_(statement, fail)                              \
    GTEST_AMBIGUOUS_ELSE_BLOCKER_                                         \
    if (::testing::internal::AlwaysTrue()) {                              \
        try {                                                             \
            GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);    \
        } catch (const std::exception& e) {                               \
            fail("Expected: " #statement " doesn't throw an exception.\n" \
                 "  Actual: it throws.")                                  \
                    << e.what();                                          \
        } catch (...) {                                                   \
            fail("Expected: " #statement " doesn't throw an exception.\n" \
                 "  Actual: it throws.");                                 \
        }                                                                 \
    }

namespace {

ELFHeader createTemplateFileHeader() {
    ELFHeader fileHeader{};

    fileHeader.e_ident[EI_MAG0] = ELFMAG0;
    fileHeader.e_ident[EI_MAG1] = ELFMAG1;
    fileHeader.e_ident[EI_MAG2] = ELFMAG2;
    fileHeader.e_ident[EI_MAG3] = ELFMAG3;
    fileHeader.e_ident[EI_CLASS] = ELFCLASS64;
    fileHeader.e_ident[EI_DATA] = ELFDATA2LSB;
    fileHeader.e_ident[EI_VERSION] = EV_NONE;
    fileHeader.e_ident[EI_OSABI] = 0;
    fileHeader.e_ident[EI_ABIVERSION] = 0;

    fileHeader.e_type = ET_REL;
    fileHeader.e_machine = EM_NONE;
    fileHeader.e_version = EV_NONE;

    fileHeader.e_entry = 0;
    fileHeader.e_flags = 0;
    fileHeader.e_shoff = sizeof(ELFHeader);
    fileHeader.e_shstrndx = 0;
    fileHeader.e_shnum = 0;

    fileHeader.e_ehsize = sizeof(ELFHeader);
    fileHeader.e_shentsize = sizeof(SectionHeader);

    return fileHeader;
}

constexpr size_t headerTableSize = 3;
constexpr size_t indexToCheck = 1;
constexpr size_t secHeaderStrIdxSecSize = 1;

}  // namespace

TEST(ELFReaderTests, ELFReaderThrowsOnIncorrectMagic) {
    auto fileHeader = createTemplateFileHeader();
    fileHeader.e_ident[EI_MAG3] = 'D';
    auto accessor =
            DDRAccessManager<elf::DDRAlwaysEmplace>(reinterpret_cast<uint8_t*>(&fileHeader), sizeof(fileHeader));

    ASSERT_ANY_THROW(auto reader = Reader<ELF_Bitness::Elf64>(&accessor));
}

TEST(ELFReaderTests, ReadingTheCorrectELFHeaderDoesntThrow) {
    std::vector<SectionHeader> sectionHeaders(headerTableSize);

    auto fileHeader = createTemplateFileHeader();
    fileHeader.e_shnum = headerTableSize;
    auto secHeaderStrIdx = headerTableSize - 1;
    fileHeader.e_shstrndx = secHeaderStrIdx;
    const auto sectionTableBytes = sizeof(SectionHeader) * headerTableSize;
    const auto sectionNamesOffset = sizeof(fileHeader) + sectionTableBytes;
    sectionHeaders[indexToCheck].sh_offset = sizeof(fileHeader);
    sectionHeaders[secHeaderStrIdx].sh_type = SHT_STRTAB;
    sectionHeaders[secHeaderStrIdx].sh_offset = sectionNamesOffset;
    sectionHeaders[secHeaderStrIdx].sh_size = secHeaderStrIdxSecSize;

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&fileHeader),
                  reinterpret_cast<uint8_t*>(&fileHeader) + sizeof(fileHeader));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(sectionHeaders.data()),
                  reinterpret_cast<uint8_t*>(sectionHeaders.data()) + sizeof(SectionHeader) * headerTableSize);
    buffer.push_back('\0');
    auto accessor = DDRAccessManager<elf::DDRAlwaysEmplace>(buffer.data(), buffer.size());

    OV_ASSERT_NO_THROW(auto reader = Reader<ELF_Bitness::Elf64>(&accessor));
}

TEST(ELFReaderTests, ELFHeaderIsReadCorrectly) {
    std::vector<SectionHeader> sectionHeaders(headerTableSize);

    auto fileHeader = createTemplateFileHeader();
    fileHeader.e_shnum = headerTableSize;
    auto secHeaderStrIdx = headerTableSize - 1;
    fileHeader.e_shstrndx = secHeaderStrIdx;
    const auto sectionTableBytes = sizeof(SectionHeader) * headerTableSize;
    const auto sectionNamesOffset = sizeof(fileHeader) + sectionTableBytes;
    sectionHeaders[indexToCheck].sh_offset = sizeof(fileHeader);
    sectionHeaders[secHeaderStrIdx].sh_type = SHT_STRTAB;
    sectionHeaders[secHeaderStrIdx].sh_offset = sectionNamesOffset;
    sectionHeaders[secHeaderStrIdx].sh_size = secHeaderStrIdxSecSize;

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&fileHeader),
                  reinterpret_cast<uint8_t*>(&fileHeader) + sizeof(fileHeader));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(sectionHeaders.data()),
                  reinterpret_cast<uint8_t*>(sectionHeaders.data()) + sizeof(SectionHeader) * headerTableSize);
    buffer.push_back('\0');
    auto accessor = DDRAccessManager<elf::DDRAlwaysEmplace>(buffer.data(), buffer.size());

    const auto reader = Reader<ELF_Bitness::Elf64>(&accessor);
    auto parsedFileHeader = *reader.getHeader();

    ASSERT_TRUE(sizeof(fileHeader) == sizeof(parsedFileHeader));
    ASSERT_TRUE(!memcmp(&fileHeader, &parsedFileHeader, sizeof(parsedFileHeader)));
}

TEST(ELFReaderTests, ELFReaderThrowsOnInvalidSectionHeaderCount) {
    auto fileHeader = createTemplateFileHeader();
    fileHeader.e_shnum = 1;
    auto accessor =
            DDRAccessManager<elf::DDRAlwaysEmplace>(reinterpret_cast<uint8_t*>(&fileHeader), sizeof(fileHeader));

    ASSERT_ANY_THROW(auto reader = Reader<ELF_Bitness::Elf64>(&accessor));
}

TEST(ELFReaderTests, SectionHeadersAreReadCorrectly) {
    std::vector<SectionHeader> sectionHeaders(headerTableSize);

    for (size_t idx = 0; idx < sectionHeaders.size(); idx++) {
        sectionHeaders[idx].sh_name = idx;
        sectionHeaders[idx].sh_size = headerTableSize;
    }

    auto fileHeader = createTemplateFileHeader();
    fileHeader.e_shnum = headerTableSize;
    fileHeader.e_shstrndx = headerTableSize - 1;
    const auto sectionTableBytes = sizeof(SectionHeader) * headerTableSize;
    const auto sectionNamesOffset = sizeof(fileHeader) + sectionTableBytes;
    sectionHeaders[fileHeader.e_shstrndx].sh_type = SHT_STRTAB;
    sectionHeaders[fileHeader.e_shstrndx].sh_offset = sectionNamesOffset;
    sectionHeaders[fileHeader.e_shstrndx].sh_size = headerTableSize;

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&fileHeader),
                  reinterpret_cast<uint8_t*>(&fileHeader) + sizeof(fileHeader));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(sectionHeaders.data()),
                  reinterpret_cast<uint8_t*>(sectionHeaders.data()) + sizeof(SectionHeader) * headerTableSize);
    buffer.insert(buffer.end(), headerTableSize, '\0');

    auto accessor = DDRAccessManager<elf::DDRAlwaysEmplace>(buffer.data(), buffer.size());
    auto reader = Reader<ELF_Bitness::Elf64>(&accessor);

    ASSERT_TRUE(sizeof(sectionHeaders[0]) == sizeof(*reader.getSection(0).getHeader()));

    for (uint64_t idx = 0; idx < sectionHeaders.size(); ++idx) {
        ASSERT_TRUE(!memcmp(&sectionHeaders[idx], reader.getSection(idx).getHeader(), sizeof(sectionHeaders[0])));
    }
}

TEST(ELFReaderTests, PointerToSectionDataIsResolvedCorrectly) {
    std::vector<SectionHeader> sectionHeaders(headerTableSize);

    for (size_t idx = 0; idx < sectionHeaders.size(); idx++) {
        sectionHeaders[idx].sh_name = idx;
        sectionHeaders[idx].sh_size = headerTableSize;
    }

    auto fileHeader = createTemplateFileHeader();
    fileHeader.e_shnum = headerTableSize;
    fileHeader.e_shstrndx = headerTableSize - 1;
    const auto sectionTableBytes = sizeof(SectionHeader) * headerTableSize;
    const auto sectionNamesOffset = sizeof(fileHeader) + sectionTableBytes;
    sectionHeaders[indexToCheck].sh_offset = sizeof(fileHeader);
    sectionHeaders[fileHeader.e_shstrndx].sh_type = SHT_STRTAB;
    sectionHeaders[fileHeader.e_shstrndx].sh_offset = sectionNamesOffset;
    sectionHeaders[fileHeader.e_shstrndx].sh_size = headerTableSize;

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&fileHeader),
                  reinterpret_cast<uint8_t*>(&fileHeader) + sizeof(fileHeader));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(sectionHeaders.data()),
                  reinterpret_cast<uint8_t*>(sectionHeaders.data()) + sizeof(SectionHeader) * headerTableSize);
    buffer.insert(buffer.end(), headerTableSize, '\0');

    auto accessor = DDRAccessManager<elf::DDRAlwaysEmplace>(buffer.data(), buffer.size());
    auto reader = Reader<ELF_Bitness::Elf64>(&accessor);
    ASSERT_EQ(reader.getSection(indexToCheck).getData<uint8_t>(), buffer.data() + sizeof(fileHeader));
}

TEST(ELFReaderTests, PtrToSectionDataIsResolvedCorrectlyWithGetSectionNoData) {
    std::vector<SectionHeader> sectionHeaders(headerTableSize);

    for (size_t idx = 0; idx < sectionHeaders.size(); idx++) {
        sectionHeaders[idx].sh_name = idx;
        sectionHeaders[idx].sh_size = headerTableSize;
    }

    auto fileHeader = createTemplateFileHeader();
    fileHeader.e_shnum = headerTableSize;
    fileHeader.e_shstrndx = headerTableSize - 1;
    const auto sectionTableBytes = sizeof(SectionHeader) * headerTableSize;
    const auto sectionNamesOffset = sizeof(fileHeader) + sectionTableBytes;
    sectionHeaders[indexToCheck].sh_offset = sizeof(fileHeader);
    sectionHeaders[fileHeader.e_shstrndx].sh_type = SHT_STRTAB;
    sectionHeaders[fileHeader.e_shstrndx].sh_offset = sectionNamesOffset;
    sectionHeaders[fileHeader.e_shstrndx].sh_size = headerTableSize;

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&fileHeader),
                  reinterpret_cast<uint8_t*>(&fileHeader) + sizeof(fileHeader));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(sectionHeaders.data()),
                  reinterpret_cast<uint8_t*>(sectionHeaders.data()) + sizeof(SectionHeader) * headerTableSize);
    buffer.insert(buffer.end(), headerTableSize, '\0');

    auto accessor = DDRAccessManager<elf::DDRAlwaysEmplace>(buffer.data(), buffer.size());
    auto reader = Reader<ELF_Bitness::Elf64>(&accessor);
    ASSERT_EQ(reader.getSection(indexToCheck).getData<uint8_t>(), buffer.data() + sizeof(fileHeader));
}

TEST(ELFReaderTests, EntriesNumCanBeInflatedByMalformedSectionEntSize) {
    std::vector<SectionHeader> sectionHeaders(headerTableSize);

    constexpr size_t manipulatedSectionIdx = 1;
    constexpr size_t manipulatedSectionSize = sizeof(SymbolEntry);

    auto fileHeader = createTemplateFileHeader();
    fileHeader.e_shnum = headerTableSize;
    fileHeader.e_shstrndx = headerTableSize - 1;

    const auto sectionTableBytes = sizeof(SectionHeader) * headerTableSize;
    const auto manipulatedSectionOffset = sizeof(fileHeader) + sectionTableBytes;
    const auto sectionNamesOffset = manipulatedSectionOffset + manipulatedSectionSize;

    sectionHeaders[manipulatedSectionIdx].sh_name = 0;
    sectionHeaders[manipulatedSectionIdx].sh_type = SHT_SYMTAB;
    sectionHeaders[manipulatedSectionIdx].sh_offset = manipulatedSectionOffset;
    sectionHeaders[manipulatedSectionIdx].sh_size = manipulatedSectionSize;
    // Malformed value: table is interpreted as bytes instead of SymbolEntry-sized entries.
    sectionHeaders[manipulatedSectionIdx].sh_entsize = 1;

    sectionHeaders[fileHeader.e_shstrndx].sh_name = 0;
    sectionHeaders[fileHeader.e_shstrndx].sh_type = SHT_STRTAB;
    sectionHeaders[fileHeader.e_shstrndx].sh_offset = sectionNamesOffset;
    sectionHeaders[fileHeader.e_shstrndx].sh_size = 1;

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&fileHeader),
                  reinterpret_cast<uint8_t*>(&fileHeader) + sizeof(fileHeader));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(sectionHeaders.data()),
                  reinterpret_cast<uint8_t*>(sectionHeaders.data()) + sectionTableBytes);
    buffer.insert(buffer.end(), manipulatedSectionSize, 0);
    buffer.push_back('\0');

    auto accessor = DDRAccessManager<elf::DDRAlwaysEmplace>(buffer.data(), buffer.size());
    auto reader = Reader<ELF_Bitness::Elf64>(&accessor);

    const auto& section = reader.getSection(manipulatedSectionIdx);
    ASSERT_THROW(section.getEntriesNum<SymbolEntry>(), SectionError);
}

TEST(ELFReaderTests, ReaderThrowsWhenSectionNameOffsetExceedsStringTable) {
    std::vector<SectionHeader> sectionHeaders(headerTableSize);

    constexpr size_t manipulatedSectionIdx = 1;
    constexpr Elf_Word outOfBoundsNameOffset = 2;

    auto fileHeader = createTemplateFileHeader();
    fileHeader.e_shnum = headerTableSize;
    fileHeader.e_shstrndx = headerTableSize - 1;

    const auto sectionTableBytes = sizeof(SectionHeader) * headerTableSize;
    const auto sectionNamesOffset = sizeof(fileHeader) + sectionTableBytes;

    // Corrupt one section header so its name points past the section-name string table.
    sectionHeaders[manipulatedSectionIdx].sh_name = outOfBoundsNameOffset;

    // Minimal .shstrtab: one-byte table containing only '\0'.
    // Any non-zero name offset must therefore be treated as out of bounds.
    sectionHeaders[fileHeader.e_shstrndx].sh_name = 0;
    sectionHeaders[fileHeader.e_shstrndx].sh_type = SHT_STRTAB;
    sectionHeaders[fileHeader.e_shstrndx].sh_offset = sectionNamesOffset;
    sectionHeaders[fileHeader.e_shstrndx].sh_size = secHeaderStrIdxSecSize;

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&fileHeader),
                  reinterpret_cast<uint8_t*>(&fileHeader) + sizeof(fileHeader));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(sectionHeaders.data()),
                  reinterpret_cast<uint8_t*>(sectionHeaders.data()) + sectionTableBytes);
    // Backing storage for the one-byte section-name string table.
    buffer.push_back('\0');

    auto accessor = DDRAccessManager<elf::DDRAlwaysEmplace>(buffer.data(), buffer.size());

    // Reader validates section-name offsets during construction and must reject this ELF.
    ASSERT_ANY_THROW((Reader<ELF_Bitness::Elf64>(&accessor)));
}

TEST(ELFReaderTests, ReaderThrowsWhenSectionNameIsNotNullTerminatedInStringTable) {
    std::vector<SectionHeader> sectionHeaders(headerTableSize);

    constexpr size_t manipulatedSectionIdx = 1;

    auto fileHeader = createTemplateFileHeader();
    fileHeader.e_shnum = headerTableSize;
    fileHeader.e_shstrndx = headerTableSize - 1;

    const auto sectionTableBytes = sizeof(SectionHeader) * headerTableSize;
    const auto sectionNamesOffset = sizeof(fileHeader) + sectionTableBytes;

    // Offset is in-range, but points to a string that is not null-terminated in .shstrtab.
    sectionHeaders[manipulatedSectionIdx].sh_name = 1;

    sectionHeaders[fileHeader.e_shstrndx].sh_name = 0;
    sectionHeaders[fileHeader.e_shstrndx].sh_type = SHT_STRTAB;
    sectionHeaders[fileHeader.e_shstrndx].sh_offset = sectionNamesOffset;
    sectionHeaders[fileHeader.e_shstrndx].sh_size = 2;

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&fileHeader),
                  reinterpret_cast<uint8_t*>(&fileHeader) + sizeof(fileHeader));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(sectionHeaders.data()),
                  reinterpret_cast<uint8_t*>(sectionHeaders.data()) + sectionTableBytes);
    // .shstrtab is valid at offset 0 ('\0'), but missing a terminator for the name at offset 1.
    buffer.push_back('\0');
    buffer.push_back('A');

    auto accessor = DDRAccessManager<elf::DDRAlwaysEmplace>(buffer.data(), buffer.size());

    ASSERT_ANY_THROW((Reader<ELF_Bitness::Elf64>(&accessor)));
}
