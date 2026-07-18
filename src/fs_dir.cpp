#include "fs.h"
#include <cstring>

const int ENTRIES_PER_BLOCK = BLOCK_SIZE / sizeof(DirEntry);

int FileSystem::findEntry(int dirInode, const std::string& name) const {
    Inode dir = readInode(dirInode);
    char buf[BLOCK_SIZE];

    for (int d = 0; d < NUM_DIRECT; d++) {
        if (dir.direct[d] == -1) continue;
        const_cast<Disk&>(disk).readBlock(dir.direct[d], buf);
        DirEntry* entries = reinterpret_cast<DirEntry*>(buf);
        for (int e = 0; e < ENTRIES_PER_BLOCK; e++) {
            if (entries[e].inode_num != -1 && name == entries[e].name) {
                return entries[e].inode_num;
            }
        }
    }
    return -1;
}

bool FileSystem::addEntry(int dirInode, const std::string& name, int inodeNum) {
    Inode dir = readInode(dirInode);
    char buf[BLOCK_SIZE];

    // try to find a free slot in existing blocks
    for (int d = 0; d < NUM_DIRECT; d++) {
        if (dir.direct[d] == -1) continue;
        disk.readBlock(dir.direct[d], buf);
        DirEntry* entries = reinterpret_cast<DirEntry*>(buf);
        for (int e = 0; e < ENTRIES_PER_BLOCK; e++) {
            if (entries[e].inode_num == -1) {
                memset(entries[e].name, 0, MAX_NAME_LEN);
                strncpy(entries[e].name, name.c_str(), MAX_NAME_LEN - 1);
                entries[e].inode_num = inodeNum;
                disk.writeBlock(dir.direct[d], buf);
                return true;
            }
        }
    }

    // need a new block
    for (int d = 0; d < NUM_DIRECT; d++) {
        if (dir.direct[d] == -1) {
            int newBlock = allocBlock();
            if (newBlock == -1) return false; // disk full
            dir.direct[d] = newBlock;

            memset(buf, 0, BLOCK_SIZE);
            DirEntry* entries = reinterpret_cast<DirEntry*>(buf);
            for (int e = 0; e < ENTRIES_PER_BLOCK; e++) entries[e].inode_num = -1;

            memset(entries[0].name, 0, MAX_NAME_LEN);
            strncpy(entries[0].name, name.c_str(), MAX_NAME_LEN - 1);
            entries[0].inode_num = inodeNum;

            disk.writeBlock(newBlock, buf);
            writeInode(dirInode, dir);
            return true;
        }
    }
    return false; // directory full (no direct slots left) - simplified fs limitation
}

bool FileSystem::removeEntry(int dirInode, const std::string& name) {
    Inode dir = readInode(dirInode);
    char buf[BLOCK_SIZE];

    for (int d = 0; d < NUM_DIRECT; d++) {
        if (dir.direct[d] == -1) continue;
        disk.readBlock(dir.direct[d], buf);
        DirEntry* entries = reinterpret_cast<DirEntry*>(buf);
        for (int e = 0; e < ENTRIES_PER_BLOCK; e++) {
            if (entries[e].inode_num != -1 && name == entries[e].name) {
                entries[e].inode_num = -1;
                disk.writeBlock(dir.direct[d], buf);
                return true;
            }
        }
    }
    return false;
}

bool FileSystem::isDirEmpty(int dirInode) const {
    Inode dir = readInode(dirInode);
    char buf[BLOCK_SIZE];
    int count = 0;

    for (int d = 0; d < NUM_DIRECT; d++) {
        if (dir.direct[d] == -1) continue;
        const_cast<Disk&>(disk).readBlock(dir.direct[d], buf);
        DirEntry* entries = reinterpret_cast<DirEntry*>(buf);
        for (int e = 0; e < ENTRIES_PER_BLOCK; e++) {
            if (entries[e].inode_num != -1) {
                count++;
                std::string n = entries[e].name;
                if (n != "." && n != "..") return false; // has real content
            }
        }
    }
    return true;
}

void FileSystem::listEntries(int dirInode, std::vector<DirEntry>& out) const {
    Inode dir = readInode(dirInode);
    char buf[BLOCK_SIZE];

    for (int d = 0; d < NUM_DIRECT; d++) {
        if (dir.direct[d] == -1) continue;
        const_cast<Disk&>(disk).readBlock(dir.direct[d], buf);
        DirEntry* entries = reinterpret_cast<DirEntry*>(buf);
        for (int e = 0; e < ENTRIES_PER_BLOCK; e++) {
            if (entries[e].inode_num != -1) out.push_back(entries[e]);
        }
    }
}
