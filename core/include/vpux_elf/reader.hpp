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
#include <vpux_elf/types/section_header.hpp>
#include <vpux_elf/types/program_header.hpp>
#include <vpux_elf/types/elf_header.hpp>
#include <vpux_elf/types/elf_structs.hpp>
#include <vpux_elf/utils/error.hpp>
#include <vpux_elf/utils/utils.hpp>
#include <vpux_elf/utils/error.hpp>


#include <string>
#include <vector>

namespace elf {

template<ELF_Bitness B>
class Reader {
public:
    class Section {
    public:
        Section() = delete;
        Section(const typename ElfTypes<B>::SectionHeader* sectionHeader, const uint8_t* data, const char* name) :
            m_sectionHeader(sectionHeader), m_data(data), m_name(name) {}

        const typename ElfTypes<B>::SectionHeader* getHeader() const {
            return m_sectionHeader;
        }

        size_t getEntriesNum() const {
            return static_cast<size_t>(m_sectionHeader->sh_size / m_sectionHeader->sh_entsize);
        }

        const char* getName() const {
            return m_name;
        }

        template<typename T>
        const T* getData() const {
            return reinterpret_cast<const T*>(m_data);
        }

    private:
        const typename ElfTypes<B>::SectionHeader* m_sectionHeader;
        const uint8_t* m_data;
        const char* m_name;
    };

    class Segment {
    public:
        Segment() = delete;
        Segment(const typename ElfTypes<B>::ProgramHeader* programHeader, const uint8_t* data) : m_programHeader(programHeader), m_data(data) {}

        const typename ElfTypes<B>::ProgramHeader* getHeader() const {
            return m_programHeader;
        }

        const uint8_t* getData() const {
            return m_data;
        }

    private:
        Reader* m_reader;
        const typename ElfTypes<B>::ProgramHeader* m_programHeader;
        const uint8_t* m_data;
    };

public:
    Reader(const uint8_t* blob, size_t size) : m_blob(blob), m_size(size), m_elfHeader(reinterpret_cast<decltype(m_elfHeader)>(blob)) {

        if (utils::checkELFMagic(m_blob) != true) {
            VPUX_ELF_THROW(HeaderError, "Incorrect ELF magic");
        }

        m_sectionHeadersStart = reinterpret_cast<const typename ElfTypes<B>::SectionHeader*>(m_blob + m_elfHeader->e_shoff);
        m_programHeadersStart = reinterpret_cast<const typename ElfTypes<B>::ProgramHeader*>(m_blob + m_elfHeader->e_phoff);
        m_sectionHeadersNames = reinterpret_cast<const char*>(m_blob + (m_sectionHeadersStart + m_elfHeader->e_shstrndx)->sh_offset);
    }

    const uint8_t* getBlob() const {
        return m_blob;
    }

    size_t getSize() const {
        return m_size;
    }

    const typename ElfTypes<B>::ELFHeader* getHeader() const {
        return m_elfHeader;
    }

    size_t getSectionsNum() const {
        return m_elfHeader->e_shnum;
    }

    size_t getSegmentsNum() const {
        return m_elfHeader->e_phnum;
    }

    Section getSection(size_t index) {
        const auto sectionHeader = m_sectionHeadersStart + index;
        auto data = m_blob + sectionHeader->sh_offset;
        const auto name = m_sectionHeadersNames + sectionHeader->sh_name;

        return {sectionHeader, data, name};
    }

    Segment getSegment(size_t index) {
        const auto programHeader = m_programHeadersStart + index;
        auto data = m_blob + programHeader->p_offset;

        return {programHeader, data};
    }

private:
    const uint8_t* m_blob = nullptr;
    const size_t m_size;

    const typename ElfTypes<B>::ELFHeader* m_elfHeader = nullptr;
    const typename ElfTypes<B>::SectionHeader* m_sectionHeadersStart = nullptr;
    const typename ElfTypes<B>::ProgramHeader* m_programHeadersStart = nullptr;
    const char* m_sectionHeadersNames = nullptr;
};

} // namespace elf
