#ifndef FS_H
#define FS_H

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

// ------------------- Disk Layout Constants -------------------
const int BLOCK_SIZE   = 512;                 // bytes per block
const int NUM_BLOCKS   = 20480;               // total blocks -> 10MB virtual disk
const int NUM_INODES   = 1024;                // max files+dirs
const int MAX_NAME_LEN = 28;                  // max filename length
const int NUM_DIRECT   = 12;                  // direct block pointers per inode
const int PTRS_PER_BLOCK = BLOCK_SIZE / sizeof(int); // indirect pointers per block

const int MODE_FREE = 0;
const int MODE_FILE = 1;
const int MODE_DIR  = 2;

const int MAGIC_NUMBER = 0x4D494E49; // "MINI"

// ------------------- On-Disk Structures -------------------

struct Superblock {
    int magic;
    int block_size;
    int num_blocks;
    int num_inodes;
    int inode_bitmap_start;
    int inode_bitmap_blocks;
    int block_bitmap_start;
    int block_bitmap_blocks;
    int inode_table_start;
    int inode_table_blocks;
    int data_block_start;
    int root_inode;
};

struct Inode {
    int mode;                    // MODE_FREE / MODE_FILE / MODE_DIR
    int size;                    // size in bytes
    int direct[NUM_DIRECT];      // direct block pointers (-1 if unused)
    int indirect;                // single indirect block pointer (-1 if unused)
    int used;                    // 1 if allocated
};

struct DirEntry {
    char name[MAX_NAME_LEN];
    int inode_num;               // -1 = empty slot
};

// ------------------- Disk: raw block I/O -------------------

class Disk {
public:
    bool create(const std::string& path, int totalBlocks); // format a new disk.img
    bool open(const std::string& path);                    // open existing disk.img
    void readBlock(int blockNum, void* buf);
    void writeBlock(int blockNum, const void* buf);
    void close();
    bool isOpen() const { return file.is_open(); }

private:
    std::fstream file;
};

// ------------------- FileSystem: high-level operations -------------------

class FileSystem {
public:
    bool format(const std::string& diskPath);   // create brand-new fs
    bool mount(const std::string& diskPath);     // load existing fs
    void unmount();

    // navigation
    void pwd() const;
    bool cd(const std::string& name);
    void ls() const;
    void tree() const;

    // file/dir operations
    bool mkdir(const std::string& name);
    bool rmdir(const std::string& name);
    bool touch(const std::string& name);
    bool writeFile(const std::string& name, const std::string& content, bool append);
    bool catFile(const std::string& name) const;
    bool rm(const std::string& name);
    bool stat(const std::string& name) const;
    void dfInfo() const; // disk free space info

private:
    Disk disk;
    Superblock sb;
    std::vector<uint8_t> inodeBitmap;
    std::vector<uint8_t> blockBitmap;
    std::vector<std::pair<std::string,int>> pathStack; // {"/",0} , {"foo", inodeNum}, ...

    // bitmap helpers
    bool getBit(std::vector<uint8_t>& bitmap, int idx) const;
    void setBit(std::vector<uint8_t>& bitmap, int idx, bool val);
    void loadBitmaps();
    void saveInodeBitmap();
    void saveBlockBitmap();

    // inode helpers
    Inode readInode(int idx) const;
    void writeInode(int idx, const Inode& inode);
    int allocInode();
    void freeInode(int idx);

    // block helpers
    int allocBlock();
    void freeBlock(int blockNum);

    // directory helpers
    int findEntry(int dirInode, const std::string& name) const;
    bool addEntry(int dirInode, const std::string& name, int inodeNum);
    bool removeEntry(int dirInode, const std::string& name);
    bool isDirEmpty(int dirInode) const;
    void listEntries(int dirInode, std::vector<DirEntry>& out) const;

    // data helpers (read/write raw bytes of a file's inode)
    std::string readInodeData(const Inode& inode) const;
    bool writeInodeData(int inodeIdx, Inode& inode, const std::string& data);
    void freeInodeData(Inode& inode);

    void treeHelper(int dirInode, const std::string& prefix) const;
    int getDirSize(int dirInode) const;

    int currentInode() const { return pathStack.back().second; }
};

#endif
