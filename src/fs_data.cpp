#include "fs.h"
#include <cstring>

std::string FileSystem::readInodeData(const Inode& inode) const {
    std::string data;
    data.reserve(inode.size);
    int remaining = inode.size;
    char buf[BLOCK_SIZE];

    // direct blocks
    for (int d = 0; d < NUM_DIRECT && remaining > 0; d++) {
        if (inode.direct[d] == -1) break;
        const_cast<Disk&>(disk).readBlock(inode.direct[d], buf);
        int chunk = std::min(remaining, BLOCK_SIZE);
        data.append(buf, chunk);
        remaining -= chunk;
    }

    // indirect block
    if (remaining > 0 && inode.indirect != -1) {
        int ptrs[PTRS_PER_BLOCK];
        const_cast<Disk&>(disk).readBlock(inode.indirect, ptrs);
        for (int p = 0; p < PTRS_PER_BLOCK && remaining > 0; p++) {
            if (ptrs[p] == -1) break;
            const_cast<Disk&>(disk).readBlock(ptrs[p], buf);
            int chunk = std::min(remaining, BLOCK_SIZE);
            data.append(buf, chunk);
            remaining -= chunk;
        }
    }
    return data;
}

void FileSystem::freeInodeData(Inode& inode) {
    for (int d = 0; d < NUM_DIRECT; d++) {
        if (inode.direct[d] != -1) {
            freeBlock(inode.direct[d]);
            inode.direct[d] = -1;
        }
    }
    if (inode.indirect != -1) {
        int ptrs[PTRS_PER_BLOCK];
        disk.readBlock(inode.indirect, ptrs);
        for (int p = 0; p < PTRS_PER_BLOCK; p++) {
            if (ptrs[p] != -1) freeBlock(ptrs[p]);
        }
        freeBlock(inode.indirect);
        inode.indirect = -1;
    }
    inode.size = 0;
}

bool FileSystem::writeInodeData(int inodeIdx, Inode& inode, const std::string& data) {
    // free existing data first (overwrite semantics)
    freeInodeData(inode);

    int remaining = (int)data.size();
    int offset = 0;
    char buf[BLOCK_SIZE];

    // fill direct blocks
    for (int d = 0; d < NUM_DIRECT && remaining > 0; d++) {
        int newBlock = allocBlock();
        if (newBlock == -1) { writeInode(inodeIdx, inode); return false; } // disk full
        inode.direct[d] = newBlock;

        int chunk = std::min(remaining, BLOCK_SIZE);
        memset(buf, 0, BLOCK_SIZE);
        memcpy(buf, data.data() + offset, chunk);
        disk.writeBlock(newBlock, buf);

        offset += chunk;
        remaining -= chunk;
    }

    // indirect block for the rest
    if (remaining > 0) {
        int indirectBlock = allocBlock();
        if (indirectBlock == -1) { writeInode(inodeIdx, inode); return false; }
        inode.indirect = indirectBlock;

        int ptrs[PTRS_PER_BLOCK];
        for (int i = 0; i < PTRS_PER_BLOCK; i++) ptrs[i] = -1;

        for (int p = 0; p < PTRS_PER_BLOCK && remaining > 0; p++) {
            int newBlock = allocBlock();
            if (newBlock == -1) break; // disk full, truncate silently
            ptrs[p] = newBlock;

            int chunk = std::min(remaining, BLOCK_SIZE);
            memset(buf, 0, BLOCK_SIZE);
            memcpy(buf, data.data() + offset, chunk);
            disk.writeBlock(newBlock, buf);

            offset += chunk;
            remaining -= chunk;
        }
        disk.writeBlock(indirectBlock, ptrs);
    }

    inode.size = offset; // actual bytes written (may be < data.size() if disk filled up)
    writeInode(inodeIdx, inode);
    return remaining == 0;
}
