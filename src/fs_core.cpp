#include "fs.h"
#include <iostream>
#include <cstring>
#include <cmath>

static int ceilDiv(int a, int b) { return (a + b - 1) / b; }

// ------------------- Format / Mount -------------------

bool FileSystem::format(const std::string& diskPath) {
    if (!disk.create(diskPath, NUM_BLOCKS)) {
        std::cerr << "Error: could not create disk image at " << diskPath << "\n";
        return false;
    }

    sb.magic = MAGIC_NUMBER;
    sb.block_size = BLOCK_SIZE;
    sb.num_blocks = NUM_BLOCKS;
    sb.num_inodes = NUM_INODES;

    sb.inode_bitmap_start = 1;
    sb.inode_bitmap_blocks = ceilDiv(ceilDiv(NUM_INODES, 8), BLOCK_SIZE);
    if (sb.inode_bitmap_blocks == 0) sb.inode_bitmap_blocks = 1;

    sb.block_bitmap_start = sb.inode_bitmap_start + sb.inode_bitmap_blocks;
    sb.block_bitmap_blocks = ceilDiv(ceilDiv(NUM_BLOCKS, 8), BLOCK_SIZE);
    if (sb.block_bitmap_blocks == 0) sb.block_bitmap_blocks = 1;

    sb.inode_table_start = sb.block_bitmap_start + sb.block_bitmap_blocks;
    sb.inode_table_blocks = ceilDiv(NUM_INODES * (int)sizeof(Inode), BLOCK_SIZE);

    sb.data_block_start = sb.inode_table_start + sb.inode_table_blocks;
    sb.root_inode = 0;

    // write superblock to block 0
    char sbBuf[BLOCK_SIZE];
    memset(sbBuf, 0, BLOCK_SIZE);
    memcpy(sbBuf, &sb, sizeof(Superblock));
    disk.writeBlock(0, sbBuf);

    // init bitmaps in memory, all zero (free)
    inodeBitmap.assign(sb.inode_bitmap_blocks * BLOCK_SIZE, 0);
    blockBitmap.assign(sb.block_bitmap_blocks * BLOCK_SIZE, 0);

    // zero out inode table on disk
    char zeroBlock[BLOCK_SIZE];
    memset(zeroBlock, 0, BLOCK_SIZE);
    for (int i = 0; i < sb.inode_table_blocks; i++) {
        disk.writeBlock(sb.inode_table_start + i, zeroBlock);
    }

    saveInodeBitmap();
    saveBlockBitmap();

    // allocate root inode (inode 0) as a directory
    int rootIdx = allocInode();
    Inode root{};
    root.mode = MODE_DIR;
    root.size = 0;
    for (int i = 0; i < NUM_DIRECT; i++) root.direct[i] = -1;
    root.indirect = -1;
    root.used = 1;
    writeInode(rootIdx, root);

    pathStack.clear();
    pathStack.push_back({"/", rootIdx});

    // add "." and ".." entries pointing to itself
    addEntry(rootIdx, ".", rootIdx);
    addEntry(rootIdx, "..", rootIdx);

    std::cout << "Formatted new virtual disk: " << diskPath
              << " (" << NUM_BLOCKS << " blocks, " << NUM_INODES << " inodes)\n";
    std::cout << "Data blocks start at block " << sb.data_block_start
              << " / " << NUM_BLOCKS << " total\n";
    return true;
}

bool FileSystem::mount(const std::string& diskPath) {
    if (!disk.open(diskPath)) return false;

    char sbBuf[BLOCK_SIZE];
    disk.readBlock(0, sbBuf);
    memcpy(&sb, sbBuf, sizeof(Superblock));

    if (sb.magic != MAGIC_NUMBER) {
        std::cerr << "Error: not a valid mini-fs disk image.\n";
        return false;
    }

    loadBitmaps();

    pathStack.clear();
    pathStack.push_back({"/", sb.root_inode});
    return true;
}

void FileSystem::unmount() {
    disk.close();
}

// ------------------- Bitmap helpers -------------------

bool FileSystem::getBit(std::vector<uint8_t>& bitmap, int idx) const {
    return (bitmap[idx / 8] >> (idx % 8)) & 1;
}

void FileSystem::setBit(std::vector<uint8_t>& bitmap, int idx, bool val) {
    if (val) bitmap[idx / 8] |= (1 << (idx % 8));
    else     bitmap[idx / 8] &= ~(1 << (idx % 8));
}

void FileSystem::loadBitmaps() {
    inodeBitmap.assign(sb.inode_bitmap_blocks * BLOCK_SIZE, 0);
    for (int i = 0; i < sb.inode_bitmap_blocks; i++) {
        disk.readBlock(sb.inode_bitmap_start + i, &inodeBitmap[i * BLOCK_SIZE]);
    }
    blockBitmap.assign(sb.block_bitmap_blocks * BLOCK_SIZE, 0);
    for (int i = 0; i < sb.block_bitmap_blocks; i++) {
        disk.readBlock(sb.block_bitmap_start + i, &blockBitmap[i * BLOCK_SIZE]);
    }
}

void FileSystem::saveInodeBitmap() {
    for (int i = 0; i < sb.inode_bitmap_blocks; i++) {
        disk.writeBlock(sb.inode_bitmap_start + i, &inodeBitmap[i * BLOCK_SIZE]);
    }
}

void FileSystem::saveBlockBitmap() {
    for (int i = 0; i < sb.block_bitmap_blocks; i++) {
        disk.writeBlock(sb.block_bitmap_start + i, &blockBitmap[i * BLOCK_SIZE]);
    }
}

// ------------------- Inode helpers -------------------

Inode FileSystem::readInode(int idx) const {
    Inode inode{};
    int inodesPerBlock = BLOCK_SIZE / sizeof(Inode);
    int blockNum = sb.inode_table_start + idx / inodesPerBlock;
    int offset = (idx % inodesPerBlock) * sizeof(Inode);

    char buf[BLOCK_SIZE];
    const_cast<Disk&>(disk).readBlock(blockNum, buf);
    memcpy(&inode, buf + offset, sizeof(Inode));
    return inode;
}

void FileSystem::writeInode(int idx, const Inode& inode) {
    int inodesPerBlock = BLOCK_SIZE / sizeof(Inode);
    int blockNum = sb.inode_table_start + idx / inodesPerBlock;
    int offset = (idx % inodesPerBlock) * sizeof(Inode);

    char buf[BLOCK_SIZE];
    disk.readBlock(blockNum, buf);
    memcpy(buf + offset, &inode, sizeof(Inode));
    disk.writeBlock(blockNum, buf);
}

int FileSystem::allocInode() {
    for (int i = 0; i < sb.num_inodes; i++) {
        if (!getBit(inodeBitmap, i)) {
            setBit(inodeBitmap, i, true);
            saveInodeBitmap();
            return i;
        }
    }
    return -1; // no free inodes
}

void FileSystem::freeInode(int idx) {
    setBit(inodeBitmap, idx, false);
    saveInodeBitmap();
    Inode inode{};
    inode.mode = MODE_FREE;
    inode.used = 0;
    for (int i = 0; i < NUM_DIRECT; i++) inode.direct[i] = -1;
    inode.indirect = -1;
    writeInode(idx, inode);
}

// ------------------- Block helpers -------------------

int FileSystem::allocBlock() {
    int totalDataBlocks = sb.num_blocks - sb.data_block_start;
    for (int i = 0; i < totalDataBlocks; i++) {
        if (!getBit(blockBitmap, i)) {
            setBit(blockBitmap, i, true);
            saveBlockBitmap();
            int blockNum = sb.data_block_start + i;
            char zero[BLOCK_SIZE];
            memset(zero, 0, BLOCK_SIZE);
            disk.writeBlock(blockNum, zero);
            return blockNum;
        }
    }
    return -1; // disk full
}

void FileSystem::freeBlock(int blockNum) {
    int i = blockNum - sb.data_block_start;
    if (i < 0) return;
    setBit(blockBitmap, i, false);
    saveBlockBitmap();
}
