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

#include <vpux_elf/accessor.hpp>
#include <vpux_elf/utils/error.hpp>
#include <vpux_elf/utils/log.hpp>

namespace elf {

ElfDDRAccessManager::ElfDDRAccessManager(const uint8_t* blob, size_t size, BufferManager* bufferMgr) {
    VPUX_ELF_THROW_WHEN(blob == nullptr, ArgsError, "invalid binary file arg");

    m_blob = blob;
    m_size = size;
    m_bufferMgr = bufferMgr;
}

const uint8_t* ElfDDRAccessManager::read(const AccessorDescriptor& descriptor) {
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t offset: %llu, size: %%llu, procFlags: %llu, alignment: %llu", 
        descriptor.offset, descriptor.size, descriptor.procFlags, descriptor.alignment);
    return m_blob + descriptor.offset;
}

const uint8_t* ElfDDRAccessManager::getBlob() const {
    return m_blob;
}


ElfFSAccessManager::ElfFSAccessManager(const std::string& elfFileName, BufferManager* bufferMgr) 
    : m_elfStream(elfFileName, std::ifstream::binary) {
    VPUX_ELF_THROW_WHEN(!m_elfStream.good(), AccessError, std::string("unable to access binary file " + elfFileName).c_str());

    m_elfStream.seekg(0, m_elfStream.end);
    m_size = m_elfStream.tellg();
    m_bufferMgr = bufferMgr;
}

const uint8_t* ElfFSAccessManager::read(const AccessorDescriptor& descriptor) {
    VPUX_ELF_LOG(LogLevel::DEBUG,"\t offset: %llu, size: %%llu, procFlags: %llu, alignment: %llu", 
        descriptor.offset, descriptor.size, descriptor.procFlags, descriptor.alignment);

    if (descriptor.size > m_size || (descriptor.offset + descriptor.size > m_size)) {
        return nullptr;
    }

    const auto buffSpecs = BufferSpecs(descriptor.alignment, descriptor.size, descriptor.procFlags);
    auto deviceBuffer = m_bufferMgr->allocate(buffSpecs);
    auto charBuffer = reinterpret_cast<char*>(deviceBuffer.cpu_addr());

    m_elfStream.seekg(descriptor.offset, m_elfStream.beg);
    m_elfStream.read(charBuffer, descriptor.size);

    return reinterpret_cast<const uint8_t*>(charBuffer);
}

ElfFSAccessManager::~ElfFSAccessManager() {
    m_elfStream.close();
}

} // namespace elf
