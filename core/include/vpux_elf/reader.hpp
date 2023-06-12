//
// Copyright (C) 2023 Intel Corporation
// SPDX-License-Identifier: Apache 2.0
//

//

#pragma once

#include <vpux_elf/types/data_types.hpp>
#include <vpux_elf/types/section_header.hpp>
#include <vpux_elf/types/program_header.hpp>
#include <vpux_elf/types/elf_header.hpp>
#include <vpux_elf/types/elf_structs.hpp>
#include <vpux_elf/utils/error.hpp>
#include <vpux_elf/utils/utils.hpp>
#include <vpux_elf/utils/log.hpp>

#include <vpux_elf/accessor.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>

namespace elf {

template<ELF_Bitness B>
class Reader {
public:
    class Section {
    public:
        Section() = default;
        Section(AccessManager* accessor, const typename ElfTypes<B>::SectionHeader* sectionHeader, const char* name, const uint8_t* data = nullptr)
                : m_accessor(accessor), m_header(sectionHeader), m_name(name), m_data(data) {}

        const typename ElfTypes<B>::SectionHeader* getHeader() const {
            return m_header;
        }

        size_t getEntriesNum() const {
            return static_cast<size_t>(m_header->sh_size / m_header->sh_entsize);
        }

        const char* getName() const {
            return m_name;
        }

        template<typename T>
        const T* getData() const {
            if (m_data == nullptr) {
                m_data = m_accessor->read(AccessorDescriptor{m_header->sh_offset, m_header->sh_size, m_header->sh_flags, m_header->sh_addralign});
            }
            return reinterpret_cast<const T*>(m_data);
        }

    private:
        AccessManager* m_accessor = nullptr;
        const typename ElfTypes<B>::SectionHeader* m_header = nullptr;
        const char* m_name = nullptr;
        mutable const uint8_t* m_data = nullptr;
    };

    class Segment {
    public:
        Segment(const typename ElfTypes<B>::ProgramHeader* programHeader, const uint8_t* data)
                : m_programHeader(programHeader), m_data(data) {}

        const typename ElfTypes<B>::ProgramHeader* getHeader() const {
            return m_programHeader;
        }

        const uint8_t* getData() const {
            return m_data;
        }

    private:
        const typename ElfTypes<B>::ProgramHeader* m_programHeader = nullptr;
        const uint8_t* m_data = nullptr;
    };

public:
    Reader(AccessManager* accessor) {
        auto sanitizePointer = [](auto ptr, const char* errorMsg) {
            VPUX_ELF_THROW_UNLESS(ptr, ArgsError, errorMsg);
            return ptr;
        };
        auto sanitizeValue = [](auto value, auto lowerLimit, auto upperLimit, const char* errorMsg) {
            VPUX_ELF_THROW_UNLESS((value >= lowerLimit) && (value <= upperLimit), ArgsError, errorMsg);
            return value;
        };

        m_accessor = sanitizePointer(accessor, "Invalid AccessManager pointer");

        m_elfHeader = sanitizePointer(reinterpret_cast<const typename ElfTypes<B>::ELFHeader*>(m_accessor->read(
                                              AccessorDescriptor{0, sizeof(typename ElfTypes<B>::ELFHeader)})),
                                      "Invalid ELF header pointer");

        VPUX_ELF_THROW_UNLESS(utils::checkELFMagic(reinterpret_cast<const uint8_t*>(m_elfHeader)), HeaderError,
                              "Incorrect ELF magic");

        m_sectionNumber = sanitizeValue(m_elfHeader->e_shnum, 0, m_MAX_SECTION_NUMBER, "Invalid number of sections");
        m_segmentNumber = sanitizeValue(m_elfHeader->e_phnum, 0, m_MAX_SEGMENT_NUMBER, "Invalid number of segments");

        auto e_shoff = sanitizeValue(m_elfHeader->e_shoff, 0, m_MAX_OFFSET, "Invalid section header table offset");
        auto e_shentsize = sanitizeValue(m_elfHeader->e_shentsize, 0, sizeof(typename ElfTypes<B>::SectionHeader),
                                         "Invalid section header size");
        m_sectionHeadersStart = sanitizePointer(
                reinterpret_cast<const typename ElfTypes<B>::SectionHeader*>(
                        m_accessor->read(AccessorDescriptor{e_shoff, (uint64_t)(m_sectionNumber * e_shentsize)})),
                "Invalid section header start pointer");

        auto e_phoff = sanitizeValue(m_elfHeader->e_phoff, 0, m_MAX_OFFSET, "Invalid program header table offset");
        m_programHeadersStart = sanitizePointer(
                reinterpret_cast<const typename ElfTypes<B>::ProgramHeader*>(
                        m_accessor->read(AccessorDescriptor{e_phoff, sizeof(typename ElfTypes<B>::ProgramHeader)})),
                "Invalid program header start pointer");

        auto e_shstrndx =
                sanitizeValue(m_elfHeader->e_shstrndx, 0, m_MAX_SECTION_NUMBER - 1, "Invalid string section index");
        const auto secNames = sanitizePointer(
                reinterpret_cast<const typename ElfTypes<B>::SectionHeader*>(m_sectionHeadersStart + e_shstrndx),
                "Invalid string section header pointer");

        auto secNamesShOff = sanitizeValue(secNames->sh_offset, 0, m_MAX_OFFSET, "Invalid string section offset");
        auto secNamesShSize = sanitizeValue(secNames->sh_size, 0, m_MAX_SECTION_SIZE, "Invalid string section size");
        m_sectionHeadersNames = sanitizePointer(
                reinterpret_cast<const char*>(m_accessor->read(AccessorDescriptor{secNamesShOff, secNamesShSize})),
                "Invalid string section pointer");
    }

    const typename ElfTypes<B>::ELFHeader* getHeader() const {
        return m_elfHeader;
    }

    size_t getSectionsNum() const {
        return m_sectionNumber;
    }

    size_t getSegmentsNum() const {
        return m_segmentNumber;
    }

    const Section& getSection(size_t index) const {
        if (m_sectionsCache.find(index) != m_sectionsCache.end()) {
            return m_sectionsCache[index];
        }

        const auto secHeader = m_sectionHeadersStart + index;
        const auto name = m_sectionHeadersNames + secHeader->sh_name;
        const auto data = m_accessor->read(AccessorDescriptor{secHeader->sh_offset, secHeader->sh_size, secHeader->sh_flags, secHeader->sh_addralign});
        auto section = Section(m_accessor, secHeader, name, data);
        m_sectionsCache[index] = section;

        return m_sectionsCache[index];
    }

    const Section& getSectionNoData(size_t index) const {
        if (m_sectionsCache.find(index) != m_sectionsCache.end()) {
            return m_sectionsCache[index];
        }

        const auto sectionHeader = m_sectionHeadersStart + index;
        const auto name = m_sectionHeadersNames + sectionHeader->sh_name;
        auto section = Section(m_accessor, sectionHeader, name);
        m_sectionsCache[index] = section;

        return m_sectionsCache[index];
    }

private:
    AccessManager* m_accessor;

    const typename ElfTypes<B>::ELFHeader* m_elfHeader = nullptr;
    const typename ElfTypes<B>::SectionHeader* m_sectionHeadersStart = nullptr;
    const typename ElfTypes<B>::ProgramHeader* m_programHeadersStart = nullptr;
    const char* m_sectionHeadersNames = nullptr;
    size_t m_sectionNumber = 0;
    size_t m_segmentNumber = 0;

    // Reasonable limits for number of sections and segments.
    // Given how the compiler packs contents in the ELF output, the current limits should
    // be enough for all networks.
    static constexpr size_t m_MAX_SECTION_NUMBER = 1000;
    static constexpr size_t m_MAX_SEGMENT_NUMBER = 1000;

    // Assuming reasonable 64 GB offset limit
    static constexpr uint64_t m_MAX_OFFSET = 64000000000;
    // Assuming reasonable 64 GB section size limit
    static constexpr uint64_t m_MAX_SECTION_SIZE = 64000000000;

    mutable std::unordered_map<size_t, Section> m_sectionsCache;
};

} // namespace elf
