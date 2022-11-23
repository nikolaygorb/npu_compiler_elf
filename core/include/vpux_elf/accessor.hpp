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

#include <string>
#include <iostream>
#include <fstream>

#include <vpux_loader/vpux_loader.hpp>

namespace elf {

/*
Abstraction class to encapsulate access to ELF binary file from DDR memory.
*/

class ElfDDRAccessManager : public AccessManager {
public:
    ElfDDRAccessManager(const uint8_t* blob, size_t size, BufferManager* bufferMgr = nullptr);

    const uint8_t* read(const AccessorDescriptor& descriptor) override;
    const uint8_t* getBlob() const;

private:
    const uint8_t* m_blob = nullptr;
};

class ElfFSAccessManager : public AccessManager {
public:
    ElfFSAccessManager(const std::string& elfFileName, BufferManager* bufferMgr);
 
    const uint8_t* read(const AccessorDescriptor& descriptor) override;

    ~ElfFSAccessManager();

private:
    std::ifstream m_elfStream;
};

} // namespace elf
