# Mini File System

A small Unix-style file system implemented from scratch in **C++17**. It uses a single file as a virtual disk and implements its own block allocation, inode management, directory entries, file I/O, and command-line interface.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus)
![Build](https://img.shields.io/badge/build-Make-brightgreen)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

## Overview

The goal of this project is to understand what happens underneath high-level file operations such as `open`, `read`, `write`, and `mkdir`.

Instead of using the host operating system's normal directory structure for the file system itself, the project stores its own metadata and file contents inside a **10 MB virtual disk image** (`disk.img`). The file system can then be closed and mounted again without losing its state.

The design is inspired by classic Unix/ext2-style concepts, while remaining intentionally small enough to understand and experiment with.

## Features

- **Persistent virtual disk** backed by a single `.img` file
- **Superblock** containing file-system layout information
- **Inode table** for files and directories
- **Separate inode and block bitmaps** for allocation tracking
- **12 direct block pointers + 1 single-indirect pointer** per inode
- **Hierarchical directories** represented by directory-entry records (`name -> inode number`)
- File creation, overwrite, append, read, and deletion
- Directory creation, removal, navigation, and listing
- Recursive directory-size calculation used by both `ls` and `stat`
- Disk usage reporting with `df`
- Interactive shell-like command-line interface

## File-System Layout

The virtual disk is divided into metadata and data regions:

```text
+-----------------------+
| Superblock            |
+-----------------------+
| Inode bitmap          |
+-----------------------+
| Block bitmap          |
+-----------------------+
| Inode table           |
+-----------------------+
| Data blocks           |
| - file contents       |
| - directory entries   |
| - indirect pointers   |
+-----------------------+
```

The current configuration is:

| Property | Value |
|---|---:|
| Virtual disk size | 10 MB |
| Block size | 512 bytes |
| Total blocks | 20,480 |
| Maximum inodes | 1,024 |
| Direct pointers per inode | 12 |
| Pointers in one indirect block | 128 |
| Maximum file data size | about 71.5 KB |

## Inodes and Direct/Indirect Blocks

Each file or directory is represented by an inode. The inode stores metadata such as type, size, and block pointers; the filename is stored in its parent directory entry rather than inside the inode.

```cpp
struct Inode {
    int mode;
    int size;
    int direct[12];
    int indirect;
    int used;
};
```

For larger files, the `indirect` pointer references a block containing additional data-block addresses.

This is a simplified design: there is currently **no double-indirect addressing**, which is why the maximum file size is limited.

## Directory Representation

Directories are stored using the same underlying block mechanism as files, but their data is interpreted as directory entries:

```text
name -> inode number
```

Each directory also contains `.` and `..` entries. Commands such as `ls`, `cd`, `mkdir`, and `rmdir` operate on these entries and the associated inodes.

## Directory Size

Directory size displayed by `ls` and `stat` is the **logical total size of files contained in the directory recursively**.

For example:

```text
/docs
  notes.txt       100 bytes
  /code
    main.cpp      250 bytes
```

then the displayed size of `/docs` is:

```text
100 + 250 = 350 bytes
```

This is separate from the number of blocks physically allocated to the directory inode itself; `stat` continues to report allocated blocks independently.

## Getting Started

### Requirements

- C++17-compatible compiler (`g++` or `clang++`)
- `make`

### Build

```bash
git clone https://github.com/DragonC-der/Mini-File-System
cd Mini-File-System
make
```

An alternative build script is also provided:

```bash
./build.sh
```

### Run

```bash
./minifs
```

If no disk image exists, the program formats a new one. If an existing image is supplied, it mounts that file system.

```bash
./minifs mydisk.img
```

## Example

```text
minifs:/$ mkdir projects
minifs:/$ cd projects
minifs:/projects$ mkdir code
minifs:/projects$ write notes.txt Hello, file system!
minifs:/projects$ cd code
minifs:/projects/code$ write main.cpp int main() {}
minifs:/projects/code$ cd ..
minifs:/projects$ ls
d      13  code/
-      19  notes.txt
minifs:/projects$ stat code
Name:   code
Inode:  3
Type:   directory
Size:   13 bytes
Blocks: 1 (512 bytes)
```

The state is persisted in the disk image, so closing and reopening the program preserves the file system contents.

## Command Reference

| Command | Description |
|---|---|
| `mkdir <name>` | Create a directory |
| `rmdir <name>` | Remove an empty directory |
| `cd <name>` | Enter a directory |
| `cd ..` | Move to the parent directory |
| `pwd` | Print the current path |
| `ls` | List entries and their logical sizes |
| `tree` | Display the directory tree |
| `touch <name>` | Create an empty file |
| `write <name> <text>` | Create/overwrite a file |
| `append <name> <text>` | Append to a file |
| `cat <name>` | Print file contents |
| `rm <name>` | Remove a file |
| `stat <name>` | Display inode and size/block metadata |
| `df` | Display file-system disk usage |
| `exit` | Exit the CLI |

## Architecture

```text
                  +----------------+
                  |    main.cpp    |
                  |   CLI / REPL   |
                  +--------+-------+
                           |
                  +--------v-------+
                  |    fs_ops.cpp  |
                  | user operations |
                  +--------+-------+
                           |
          +----------------+----------------+
          |                |                |
   +------v------+   +-----v------+   +-----v------+
   | fs_dir.cpp  |   | fs_data.cpp|   | fs_core.cpp|
   | directories |   | file data  |   | metadata /  |
   | & entries   |   | read/write |   | allocation |
   +------+------+
          |                |                |
          +----------------+----------------+
                           |
                    +------v------+
                    |  disk.cpp   |
                    | block I/O   |
                    +------+------+
                           |
                     disk.img file
```

### Source files

```text
Mini-File-System-main/
├── include/
│   └── fs.h              # data structures and FileSystem declarations
├── src/
│   ├── disk.cpp          # raw block-level disk I/O
│   ├── fs_core.cpp       # format/mount, bitmaps, inode allocation
│   ├── fs_dir.cpp        # directory entry operations
│   ├── fs_data.cpp       # file data read/write and block management
│   ├── fs_ops.cpp        # user-facing file-system commands
│   └── main.cpp          # interactive CLI
├── Makefile
├── build.sh
└── README.md
```

## Limitations

This project is intentionally small and educational. The following features are not implemented:

- No journaling or crash-recovery mechanism
- No concurrency control or multi-user support
- No file permissions/ownership model
- No timestamps
- No symbolic links
- No double- or triple-indirect block addressing
- Directory growth is limited by the current directory data-block implementation

A crash in the middle of an update can therefore leave on-disk metadata inconsistent.

## Possible Extensions

- Add journaling or a write-ahead log
- Add double-indirect block pointers
- Add file permissions and timestamps
- Add symbolic links
- Add automated tests for mount/format, allocation, file operations, and recovery scenarios
- Add integrity-check and file-system consistency utilities

## What This Project Demonstrates

This project is primarily an exploration of **operating-system and file-system fundamentals**:

- block-based storage
- inodes
- bitmap allocation
- directory structures
- direct/indirect addressing
- persistence
- file and directory system calls
- separation between metadata and file names/data

It is not intended to be a production filesystem; the implementation is deliberately compact and focused on understanding the underlying mechanisms.
