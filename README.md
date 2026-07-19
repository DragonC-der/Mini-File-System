<div align="center">

# Mini File System

**A Unix-style file system built from scratch in C++** — block storage, inodes, free-space bitmaps, and directory structures, all persisted to a single flat file acting as a virtual disk, with an interactive CLI.

![C++](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus)
![Build](https://img.shields.io/badge/build-Make-brightgreen)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

</div>

---

## Table of Contents

- [Why this project](#why-this-project)
- [Features](#features)
- [Quick Start](#quick-start)
- [Architecture](#architecture)
- [Example Session](#example-session)
- [Command Reference](#command-reference)
- [Known Limitations](#known-limitations)
- [Possible Extensions](#possible-extensions)
- [Project Structure](#project-structure)

## Why this project

Most "build your own file system" portfolio projects simulate everything in memory. This one doesn't — it implements the actual on-disk structures real file systems use (ext2-style inodes with direct + indirect block pointers, bitmap-based free space tracking, directories as regular files containing name→inode mappings) and persists every operation immediately to a real file on disk, so state survives a process restart with no separate "save" step.

## Features

- 📁 Nested directories, implemented as regular files containing `(name, inode)` entries
- 💾 Files up to ~71.5 KB via 12 direct block pointers + 1 single-indirect pointer (ext2-style addressing)
- 🗺️ Bitmap-based free space tracking for both blocks and inodes
- 🔒 Full persistence — close the CLI, reopen it, everything is still there
- 🖥️ Interactive CLI — `mkdir`, `cd`, `ls`, `tree`, `touch`, `write`, `append`, `cat`, `rm`, `rmdir`, `stat`, `df`, `pwd`

## Quick Start

Requires a C++17 compiler (g++ or clang++).

```bash
git clone <your-repo-url>
cd mini-file-system
make          # builds the `minifs` binary
./minifs      # launches the CLI, creates disk.img if none exists
```

```
minifs:/$ mkdir projects
minifs:/$ cd projects
minifs:/projects$ write notes.txt Hello, file system!
minifs:/projects$ cat notes.txt
Hello, file system!
```

Or point it at a custom disk image:
```bash
./minifs mydisk.img
```

## Architecture

```
Block 0                     → Superblock
Block 1                     → Inode bitmap
Blocks 2..N                 → Block bitmap
Blocks N+1..M                → Inode table (128-byte inodes)
Blocks M+1..end              → Data blocks (file contents / directory entries)
```

**The inode** is the core abstraction — it stores type, size, and block pointers, but deliberately *not* a filename:

```cpp
struct Inode {
    int mode;              // FREE / FILE / DIR
    int size;               // bytes
    int direct[12];          // direct data block pointers
    int indirect;            // pointer to a block of 128 more pointers
    int used;
};
```

The name lives in the *parent directory's* entry instead, pointing at an inode number — that separation is what makes the directory-as-a-file design work below, and architecturally is what real hard links are built on.

**A directory** is just a file whose data blocks are packed with `DirEntry { name, inode_num }` records — the same mechanism real file systems like ext2 use. `ls` and `cat` end up reading data the same underlying way; only the interpretation differs.

## Example Session

```
minifs:/$ mkdir projects
minifs:/$ cd projects
minifs:/projects$ mkdir mini-fs
minifs:/projects$ cd mini-fs
minifs:/projects/mini-fs$ touch notes.txt
minifs:/projects/mini-fs$ write notes.txt Hello from the mini file system!
minifs:/projects/mini-fs$ cat notes.txt
Hello from the mini file system!
minifs:/projects/mini-fs$ stat notes.txt
Name:   notes.txt
Inode:  3
Type:   file
Size:   33 bytes
Blocks: 1 (512 bytes)
minifs:/projects/mini-fs$ cd /
minifs:/$ tree
/
`-- projects/
    `-- mini-fs/
        `-- notes.txt
minifs:/$ df
Disk size:    10240 KB
Block size:   512 bytes
Blocks:       4 / 20345 used
Inodes:       4 / 1024 used
Free space:   10170 KB
minifs:/$ exit
```

Exit and relaunch `./minifs` — everything above is still there, because it's read from `disk.img` on disk, not held in memory.

## Command Reference

| Command | Description |
|---|---|
| `mkdir <name>` | Create a directory |
| `rmdir <name>` | Remove an empty directory |
| `cd <name> / cd ..` | Change directory |
| `ls` | List current directory contents |
| `tree` | Show the full directory tree |
| `pwd` | Print current path |
| `touch <name>` | Create an empty file |
| `write <name> <text>` | Overwrite a file's contents |
| `append <name> <text>` | Append text to a file |
| `cat <name>` | Print file contents |
| `rm <name>` | Delete a file |
| `stat <name>` | Show inode metadata for a file/dir |
| `df` | Show disk usage summary |

## Known Limitations

Named directly rather than left for someone else to discover:

- **Max file size is bounded** by 12 direct blocks + 1 indirect block of 128 pointers (~71.5 KB with 512-byte blocks). A double-indirect pointer would remove this cap.
- **A directory can hold at most 12 blocks worth of entries** — no indirect block for directories currently. Fine for demo purposes, a real limitation worth calling out.
- **Single-user, single-process** — no concurrency control or locking.
- **No journaling** — a crash mid-write can leave a bitmap/inode inconsistency.

## Possible Extensions

- Journaling / write-ahead log for crash consistency
- Double-indirect block pointers for larger files
- File permissions and timestamps
- Symbolic links
- A defragmentation tool

## Project Structure

```
mini-file-system/
├── include/
│   └── fs.h            # struct definitions & class declarations
├── src/
│   ├── disk.cpp          # raw block-level disk I/O
│   ├── fs_core.cpp       # format/mount, bitmaps, inode alloc
│   ├── fs_dir.cpp        # directory entry management
│   ├── fs_data.cpp       # file data read/write (direct + indirect blocks)
│   ├── fs_ops.cpp        # user-facing commands (mkdir, cat, rm, etc.)
│   └── main.cpp           # CLI loop
├── Makefile
└── README.md
```

---

<div align="center">

Built as a from-scratch exploration of what's actually happening below `open()`, `read()`, and `write()`.

</div>
