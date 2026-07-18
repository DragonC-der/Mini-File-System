#include "fs.h"
#include <iostream>
#include <vector>

bool Disk::create(const std::string& path, int totalBlocks) {
    file.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    std::vector<char> zeroBlock(BLOCK_SIZE, 0);
    for (int i = 0; i < totalBlocks; i++) {
        file.write(zeroBlock.data(), BLOCK_SIZE);
    }
    file.close();
    // reopen in read/write mode for normal use
    file.open(path, std::ios::in | std::ios::out | std::ios::binary);
    return file.is_open();
}

bool Disk::open(const std::string& path) {
    file.open(path, std::ios::in | std::ios::out | std::ios::binary);
    return file.is_open();
}

void Disk::readBlock(int blockNum, void* buf) {
    file.seekg((long long)blockNum * BLOCK_SIZE, std::ios::beg);
    file.read(reinterpret_cast<char*>(buf), BLOCK_SIZE);
}

void Disk::writeBlock(int blockNum, const void* buf) {
    file.seekp((long long)blockNum * BLOCK_SIZE, std::ios::beg);
    file.write(reinterpret_cast<const char*>(buf), BLOCK_SIZE);
    file.flush();
}

void Disk::close() {
    if (file.is_open()) file.close();
}
