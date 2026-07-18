# Mini File System

A simplified Unix-like file system implemented from scratch in C++, built on top of a single flat file acting as a "virtual disk." Includes an interactive CLI for navigating and manipulating the file system.

This project implements the core building blocks of a real file system: block-based storage, inodes, bitmaps for free-space management, and directory structures — all persisted to disk so the file system survives program restarts.

## Features

- **Virtual disk** — a single `disk.img` file split into fixed-size 512-byte blocks
- **Superblock** — stores filesystem metadata (block size, inode count, layout offsets)
- **Inodes** — each file/directory has an inode with 12 direct block pointers + 1 single-indirect pointer (supports files up to ~71.5 KB with the default config)
- **Bitmaps** — free block and free inode tracking via bitmaps
- **Directories** — implemented as regular files containing `(name → inode)` entries, supporting nested directories (`.` and `..` included)
- **Persistence** — all state lives on disk; the CLI can be closed and reopened without losing data
- **CLI frontend** — `mkdir`, `cd`, `ls`, `tree`, `touch`, `write`, `append`, `cat`, `rm`, `rmdir`, `stat`, `df`, `pwd`

## Architecture

```
Block 0                     → Superblock
Block 1                     → Inode bitmap
Blocks 2..N                 → Block bitmap
Blocks N+1..M                → Inode table (128-byte inodes)
Blocks M+1..end              → Data blocks (file contents / directory entries)
```

Each **inode**:
```cpp
struct Inode {
    int mode;              // FREE / FILE / DIR
    int size;               // bytes
    int direct[12];          // direct data block pointers
    int indirect;            // pointer to a block of 128 more pointers
    int used;
};
```

Each **directory** is just a file whose data blocks are packed with `DirEntry { name, inode_num }` records — the same mechanism real file systems like ext2 use.

## Build & Run

Requires a C++17 compiler (g++ or clang++).

```bash
make          # builds the `minifs` binary
./minifs      # launches the CLI, creates disk.img in the current directory if none exists
```

Or specify a custom disk image path:
```bash
./minifs mydisk.img
```

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

## Design Notes / Known Limitations

- Max file size is bounded by 12 direct blocks + 1 indirect block of 128 pointers (~71.5 KB with 512-byte blocks). A double-indirect pointer would remove this cap — noted as a possible extension.
- A directory can hold at most 12 blocks worth of entries (no indirect block for directories currently) — fine for demo purposes, a real limitation worth calling out in interviews.
- Single-user, single-process — no concurrency control or locking.
- No journaling — a crash mid-write can leave a bitmap/inode inconsistency (this is intentionally left as a stretch goal, see below).

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
