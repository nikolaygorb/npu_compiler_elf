//
// Copyright (C) 2026 Intel Corporation.
// SPDX-License-Identifier: Apache-2.0
//

#include "allocator_utils/buffer_managers.hpp"

// ----- NullAllocBufferManager -----

elf::DeviceBuffer NullAllocBufferManager::allocate(const elf::BufferSpecs& buffSpecs) {
    (void)buffSpecs;
    return elf::DeviceBuffer();
}

void NullAllocBufferManager::deallocate(elf::DeviceBuffer& devAddress) {
    (void)devAddress;
}

void NullAllocBufferManager::lock(elf::DeviceBuffer& devAddress) {
    (void)devAddress;
}

void NullAllocBufferManager::unlock(elf::DeviceBuffer& devAddress) {
    (void)devAddress;
}

size_t NullAllocBufferManager::copy(elf::DeviceBuffer& to, const uint8_t* from, size_t count) {
    (void)to;
    (void)from;
    (void)count;
    return 0;
}

// ----- DummyBufferManager -----

elf::DeviceBuffer DummyBufferManager::allocate(const elf::BufferSpecs& buffSpecs) {
    auto addr = malloc(buffSpecs.size);
    return {reinterpret_cast<uint8_t*>(addr), reinterpret_cast<uint64_t>(addr), buffSpecs.size};
}

void DummyBufferManager::deallocate(elf::DeviceBuffer& devBuffer) {
    free(reinterpret_cast<void*>(devBuffer.cpu_addr()));
}

void DummyBufferManager::lock(elf::DeviceBuffer& devBuffer) {
    (void)devBuffer;
}

void DummyBufferManager::unlock(elf::DeviceBuffer& devBuffer) {
    (void)devBuffer;
}

size_t DummyBufferManager::copy(elf::DeviceBuffer& to, const uint8_t* from, size_t count) {
    memcpy(to.cpu_addr(), from, count);
    return count;
}

// ----- HeapBufferManager -----

HeapBufferManager::HeapBufferManager(std::string_view name): _name(name) {
}

elf::DeviceBuffer HeapBufferManager::allocate(const elf::BufferSpecs& buffSpecs) {
    auto ptr = static_cast<uint8_t*>(operator new[](sizeof(uint8_t) * buffSpecs.size,
                                                    static_cast<std::align_val_t>(buffSpecs.alignment)));
    VPUX_ELF_THROW_UNLESS(ptr, elf::RuntimeError, "Allocation failure");
    _allocations[ptr] = buffSpecs.alignment;  // Store alignment for correct deallocation

    // All allocations have CPU VA
    auto cpuAddr = reinterpret_cast<uint8_t*>(ptr);
    // Only NPU allocations have NPU VA
    // Initializing to 0 could help early detection of faulty allocation logic from loader
    auto npuAddr = static_cast<uint64_t>(0);

    // Update statistics
    ++_allocStats._currentTotalCount;
    _allocStats._currentTotalSize += buffSpecs.size;
    if (elf::utils::hasNPUAccess(buffSpecs.procFlags)) {
        npuAddr = reinterpret_cast<uint64_t>(ptr);

        ++_allocStats._totalNPUCount;
        _allocStats._totalNPUSize += buffSpecs.size;
    } else {
        ++_allocStats._totalCPUCount;
        _allocStats._totalCPUSize += buffSpecs.size;
    }

    return elf::DeviceBuffer(cpuAddr, npuAddr, buffSpecs.size);
}

void HeapBufferManager::deallocate(elf::DeviceBuffer& devBuffer) {
    auto buffAlignment = _allocations.find(devBuffer.cpu_addr());
    VPUX_ELF_THROW_WHEN(buffAlignment == _allocations.end(), elf::RuntimeError, "Buffer not found in allocations");
    operator delete[](devBuffer.cpu_addr(), static_cast<std::align_val_t>(buffAlignment->second));
    _allocations.erase(buffAlignment);

    VPUX_ELF_THROW_WHEN(_allocStats._currentTotalSize < devBuffer.size(), elf::RuntimeError,
                        "Freeing more memory than allocated");
    _allocStats._currentTotalSize -= devBuffer.size();
}

void HeapBufferManager::lock(elf::DeviceBuffer&) {
}

void HeapBufferManager::unlock(elf::DeviceBuffer&) {
}

size_t HeapBufferManager::copy(elf::DeviceBuffer& to, const uint8_t* from, size_t count) {
    std::memcpy(to.cpu_addr(), from, count);
    return count;
}

const HeapBufferManager::AllocStats& HeapBufferManager::getStats() {
    return _allocStats;
}

void HeapBufferManager::printAllocationStats() {
    std::cout << "================================================================================\n";
    std::cout << _name << " allocation statistics:\n";
    std::cout << " - Current allocated size: " << _allocStats._currentTotalSize << " bytes in "
              << _allocStats._currentTotalCount << " buffers\n";
    std::cout << " - All-time CPU allocated size: " << _allocStats._totalCPUSize << " bytes in "
              << _allocStats._totalCPUCount << " buffers\n";
    std::cout << " - All-time NPU allocated size: " << _allocStats._totalNPUSize << " bytes in "
              << _allocStats._totalNPUCount << " buffers\n";
    std::cout << "================================================================================\n";
    std::cout << std::endl;
}
