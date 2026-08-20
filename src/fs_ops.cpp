#include "fs.h"
#include <iostream>
#include <iomanip>

void FileSystem::pwd() const {
    if (pathStack.size() == 1) {
        std::cout << "/\n";
        return;
    }
    std::string path;
    for (size_t i = 1; i < pathStack.size(); i++) {
        path += "/" + pathStack[i].first;
    }
    std::cout << path << "\n";
}

bool FileSystem::cd(const std::string& name) {
    if (name == ".") return true;

    if (name == "..") {
        if (pathStack.size() > 1) pathStack.pop_back();
        return true;
    }

    int target = findEntry(currentInode(), name);
    if (target == -1) {
        std::cout << "cd: no such directory: " << name << "\n";
        return false;
    }
    Inode inode = readInode(target);
    if (inode.mode != MODE_DIR) {
        std::cout << "cd: not a directory: " << name << "\n";
        return false;
    }
    pathStack.push_back({name, target});
    return true;
}

int FileSystem::getDirSize(int dirInode) const {
    std::vector<DirEntry> entries;
    listEntries(dirInode, entries);

    int totalSize = 0;
    for (const auto& e : entries) {
        std::string n = e.name;
        if (n == "." || n == "..") continue;

        Inode inode = readInode(e.inode_num);
        if (inode.mode == MODE_DIR) {
            totalSize += getDirSize(e.inode_num);
        } else {
            totalSize += inode.size;
        }
    }
    return totalSize;
}

void FileSystem::ls() const {
    std::vector<DirEntry> entries;
    listEntries(currentInode(), entries);

    for (auto& e : entries) {
        std::string n = e.name;
        if (n == "." || n == "..") continue;
        Inode inode = readInode(e.inode_num);
        int displaySize = (inode.mode == MODE_DIR) ? getDirSize(e.inode_num) : inode.size;
        std::cout << (inode.mode == MODE_DIR ? "d " : "- ")
                   << std::setw(6) << displaySize << "  "
                   << n << (inode.mode == MODE_DIR ? "/" : "") << "\n";
    }
}

bool FileSystem::mkdir(const std::string& name) {
    if (findEntry(currentInode(), name) != -1) {
        std::cout << "mkdir: already exists: " << name << "\n";
        return false;
    }
    int newIdx = allocInode();
    if (newIdx == -1) { std::cout << "mkdir: no free inodes\n"; return false; }

    Inode dir{};
    dir.mode = MODE_DIR;
    dir.size = 0;
    for (int i = 0; i < NUM_DIRECT; i++) dir.direct[i] = -1;
    dir.indirect = -1;
    dir.used = 1;
    writeInode(newIdx, dir);

    addEntry(newIdx, ".", newIdx);
    addEntry(newIdx, "..", currentInode());
    addEntry(currentInode(), name, newIdx);
    return true;
}

bool FileSystem::rmdir(const std::string& name) {
    int target = findEntry(currentInode(), name);
    if (target == -1) { std::cout << "rmdir: no such directory: " << name << "\n"; return false; }
    Inode inode = readInode(target);
    if (inode.mode != MODE_DIR) { std::cout << "rmdir: not a directory: " << name << "\n"; return false; }
    if (!isDirEmpty(target)) { std::cout << "rmdir: directory not empty: " << name << "\n"; return false; }

    freeInodeData(inode); // frees "." and ".." dir-entry blocks
    freeInode(target);
    removeEntry(currentInode(), name);
    return true;
}

bool FileSystem::touch(const std::string& name) {
    if (findEntry(currentInode(), name) != -1) {
        std::cout << "touch: already exists: " << name << "\n";
        return false;
    }
    int newIdx = allocInode();
    if (newIdx == -1) { std::cout << "touch: no free inodes\n"; return false; }

    Inode file{};
    file.mode = MODE_FILE;
    file.size = 0;
    for (int i = 0; i < NUM_DIRECT; i++) file.direct[i] = -1;
    file.indirect = -1;
    file.used = 1;
    writeInode(newIdx, file);

    addEntry(currentInode(), name, newIdx);
    return true;
}

bool FileSystem::writeFile(const std::string& name, const std::string& content, bool append) {
    int idx = findEntry(currentInode(), name);
    if (idx == -1) {
        if (!touch(name)) return false;
        idx = findEntry(currentInode(), name);
    }
    Inode inode = readInode(idx);
    if (inode.mode != MODE_FILE) { std::cout << "write: not a file: " << name << "\n"; return false; }

    std::string finalData = content;
    if (append) {
        std::string existing = readInodeData(inode);
        finalData = existing + content;
    }
    return writeInodeData(idx, inode, finalData);
}

bool FileSystem::catFile(const std::string& name) const {
    int idx = findEntry(currentInode(), name);
    if (idx == -1) { std::cout << "cat: no such file: " << name << "\n"; return false; }
    Inode inode = readInode(idx);
    if (inode.mode != MODE_FILE) { std::cout << "cat: not a file: " << name << "\n"; return false; }
    std::cout << readInodeData(inode) << "\n";
    return true;
}

bool FileSystem::rm(const std::string& name) {
    int idx = findEntry(currentInode(), name);
    if (idx == -1) { std::cout << "rm: no such file: " << name << "\n"; return false; }
    Inode inode = readInode(idx);
    if (inode.mode != MODE_FILE) { std::cout << "rm: not a file (use rmdir): " << name << "\n"; return false; }

    freeInodeData(inode);
    freeInode(idx);
    removeEntry(currentInode(), name);
    return true;
}

bool FileSystem::stat(const std::string& name) const {
    int idx = findEntry(currentInode(), name);
    if (idx == -1) { std::cout << "stat: no such file or directory: " << name << "\n"; return false; }
    Inode inode = readInode(idx);

    int blocksUsed = 0;
    for (int i = 0; i < NUM_DIRECT; i++) if (inode.direct[i] != -1) blocksUsed++;
    if (inode.indirect != -1) {
        blocksUsed++;
        int ptrs[PTRS_PER_BLOCK];
        const_cast<Disk&>(disk).readBlock(inode.indirect, ptrs);
        for (int p = 0; p < PTRS_PER_BLOCK; p++) if (ptrs[p] != -1) blocksUsed++;
    }

    std::cout << "Name:   " << name << "\n";
    std::cout << "Inode:  " << idx << "\n";
    std::cout << "Type:   " << (inode.mode == MODE_DIR ? "directory" : "file") << "\n";
    int displaySize = (inode.mode == MODE_DIR) ? getDirSize(idx) : inode.size;
    std::cout << "Size:   " << displaySize << " bytes\n";
    std::cout << "Blocks: " << blocksUsed << " (" << blocksUsed * BLOCK_SIZE << " bytes)\n";
    return true;
}

void FileSystem::dfInfo() const {
    int totalDataBlocks = sb.num_blocks - sb.data_block_start;
    int usedBlocks = 0;
    for (int i = 0; i < totalDataBlocks; i++) {
        if (const_cast<FileSystem*>(this)->getBit(const_cast<std::vector<uint8_t>&>(blockBitmap), i)) usedBlocks++;
    }
    int usedInodes = 0;
    for (int i = 0; i < sb.num_inodes; i++) {
        if (const_cast<FileSystem*>(this)->getBit(const_cast<std::vector<uint8_t>&>(inodeBitmap), i)) usedInodes++;
    }

    std::cout << "Disk size:    " << (sb.num_blocks * BLOCK_SIZE) / 1024 << " KB\n";
    std::cout << "Block size:   " << BLOCK_SIZE << " bytes\n";
    std::cout << "Blocks:       " << usedBlocks << " / " << totalDataBlocks << " used\n";
    std::cout << "Inodes:       " << usedInodes << " / " << sb.num_inodes << " used\n";
    std::cout << "Free space:   " << ((totalDataBlocks - usedBlocks) * BLOCK_SIZE) / 1024 << " KB\n";
}

void FileSystem::treeHelper(int dirInode, const std::string& prefix) const {
    std::vector<DirEntry> entries;
    listEntries(dirInode, entries);

    std::vector<DirEntry> filtered;
    for (auto& e : entries) {
        std::string n = e.name;
        if (n != "." && n != "..") filtered.push_back(e);
    }

    for (size_t i = 0; i < filtered.size(); i++) {
        bool last = (i == filtered.size() - 1);
        Inode inode = readInode(filtered[i].inode_num);
        std::cout << prefix << (last ? "`-- " : "|-- ") << filtered[i].name
                   << (inode.mode == MODE_DIR ? "/" : "") << "\n";
        if (inode.mode == MODE_DIR) {
            treeHelper(filtered[i].inode_num, prefix + (last ? "    " : "|   "));
        }
    }
}

void FileSystem::tree() const {
    std::cout << "/\n";
    treeHelper(sb.root_inode, "");
}
