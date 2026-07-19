<div align="center">

# Mini File System

**A Unix-style file system built from scratch in C++** — block storage, inodes, free-space bitmaps, and directory structures, all persisted to a single flat file acting as a virtual disk, with a SOLID-refactored architecture and an interactive CLI.

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
- [SOLID Refactor — What Changed and Why](#solid-refactor--what-changed-and-why)
- [Example Session](#example-session)
- [Project Structure](#project-structure)
- [Known Limitations](#known-limitations)
- [Possible Extensions](#possible-extensions)

## Why this project

Most "build your own file system" portfolio projects simulate everything in memory. This one doesn't — it implements the actual on-disk structures real file systems use (ext2-style inodes with direct + indirect block pointers, bitmap-based free space tracking, directories as regular files containing name→inode mappings) and persists every operation immediately to a real file on disk, so state survives a process restart with no separate "save" step.

It also went through a real second pass: the first version worked correctly but had a single `FileSystem` class doing everything. The current version is a genuine SOLID refactor — not a checkbox exercise, but an honest one, including calling out the one principle (Liskov Substitution) that doesn't really apply here rather than forcing it in.

## Features

- 📁 Nested directories, implemented as regular files containing `(name, inode)` entries
- 💾 Files up to ~71.5 KB via 12 direct block pointers + 1 single-indirect pointer (ext2-style addressing)
- 🗺️ Bitmap-based free space tracking for both blocks and inodes
- 🔒 Full persistence — close the CLI, reopen it, everything is still there
- 🧩 Command-pattern CLI — `mkdir`, `cd`, `ls`, `tree`, `touch`, `write`, `append`, `cat`, `rm`, `rmdir`, `stat`, `df`
- 🏗️ Dependency-injected storage layer (`IStorage` interface) — swappable for an in-memory implementation in tests

## Quick Start

Requires a C++17 compiler (g++ or clang++).

```bash
git clone <your-repo-url>
cd mini-file-system
make          # or ./build.sh if you don't have make
./minifs
```

```
minifs:/$ mkdir projects
minifs:/$ cd projects
minifs:/projects$ write notes.txt Hello, file system!
minifs:/projects$ cat notes.txt
Hello, file system!
```

No `make`? See [build.sh](./build.sh) for a dependency-free compile command, or run:
```bash
g++ -std=c++17 -Iinclude -o minifs src/*.cpp && ./minifs
```

## Architecture

```
CLI (main.cpp)
   │
   ▼  command registry / ICommand
FileSystem  ───owns───▶ Superblock
   │    │
   │    └──owns──▶ IStorage (interface)
   │                    ▲
   │                    │ implements
   │                  Disk (real file I/O)
   │
   └──delegates to──▶ BlockAllocator, InodeTable, Directory, FileData
```

**On-disk layout** (computed dynamically at format time, not hardcoded):

| Region | Contents |
|---|---|
| Block 0 | Superblock — layout metadata, magic number |
| Inode bitmap | Free/used tracking for all inodes |
| Block bitmap | Free/used tracking for all data blocks |
| Inode table | Fixed-size array of all inodes |
| Data blocks | File content and directory entries |

**The inode** is the core abstraction — it stores type, size, and block pointers, but deliberately *not* a filename (that lives in the parent directory's entry, pointing at an inode number). This separation is what makes the directory-as-a-file design work, and architecturally is what real hard links are built on.

## SOLID Refactor — What Changed and Why

The first version had one `FileSystem` class managing bitmaps, inodes, directory entries, file byte layout, *and* the public API — four unrelated reasons to change, all in one place.

<details>
<summary><strong>Single Responsibility — split the God class</strong></summary>

| Class | Responsibility |
|---|---|
| `BlockAllocator` | Tracks free/used data blocks via a bitmap. Knows nothing about files or inodes. |
| `InodeTable` | Allocates, reads, and writes inodes. Knows nothing about directories or file content. |
| `Directory` | Maps names to inode numbers within a directory's data blocks. |
| `FileData` | Translates a byte stream to/from an inode's direct + indirect block pointers. |
| `FileSystem` | Orchestrates the above to implement `mkdir`, `cat`, `ls`, etc. No longer does bitmap or block-address math itself. |

Each class now has exactly one reason to change.
</details>

<details>
<summary><strong>Dependency Inversion — depend on an abstraction, not a concrete disk</strong></summary>

```cpp
class IStorage {
public:
    virtual void readBlock(int blockNum, void* buf) = 0;
    virtual void writeBlock(int blockNum, const void* buf) = 0;
    // ...
};
class Disk : public IStorage { /* real file-backed implementation */ };
```

`BlockAllocator`, `InodeTable`, `Directory`, `FileData`, and `FileSystem` all depend on `IStorage&`, injected via constructor — never on `Disk` directly:

```cpp
Disk disk;
FileSystem fs(disk);   // fs only knows it received "some IStorage"
```

The practical payoff: a `MemoryStorage` implementation (a `vector<char>` instead of a real file) could be dropped in for unit tests, and every other class would work against it completely unmodified — no test would need to touch the real filesystem.
</details>

<details>
<summary><strong>Interface Segregation — split the fat public API</strong></summary>

```cpp
class INavigator     { /* pwd, cd, ls, tree */ };
class IFileOperations { /* mkdir, touch, writeFile, cat, rm, stat, df */ };
class FileSystem : public INavigator, public IFileOperations { ... };
```

A client that only needs to navigate can depend on `INavigator&` and never even see the file-mutation methods it has no business calling.
</details>

<details>
<summary><strong>Open/Closed — command dispatch via a registry, not an if/else chain</strong></summary>

The old CLI dispatch was a growing `if (cmd == "mkdir") ... else if (cmd == "cd") ...` chain. Now every command is its own class implementing `ICommand`, registered by name:

```cpp
class MkdirCommand : public ICommand {
    bool execute(FileSystem& fs, const std::vector<std::string>& args) override {
        return fs.mkdir(args[1]);
    }
};
registry.registerCommand("mkdir", std::make_unique<MkdirCommand>());
```

Adding a new command means writing a new class and one registration line — the dispatch loop itself never changes. This is also the project's first real use of runtime polymorphism; the original version used no inheritance or virtual functions at all.
</details>

<details>
<summary><strong>Liskov Substitution — not really exercised (said plainly, not forced)</strong></summary>

There's no deep inheritance hierarchy here to violate LSP against. `ICommand` and `IStorage` implementations are simple enough (single-level, no overridden behavior that narrows preconditions or widens postconditions) that LSP isn't meaningfully being tested. Worth saying directly rather than claiming credit for a principle the code doesn't really engage with.
</details>

## Example Session

```
minifs:/$ mkdir projects
minifs:/$ cd projects
minifs:/projects$ touch notes.txt
minifs:/projects$ write notes.txt Hello from the SOLID-refactored version!
minifs:/projects$ cat notes.txt
Hello from the SOLID-refactored version!
minifs:/projects$ stat notes.txt
Name:   notes.txt
Inode:  1
Type:   file
Size:   41 bytes
Blocks: 1 (512 bytes)
minifs:/projects$ tree
/
`-- projects/
    `-- notes.txt
minifs:/projects$ exit
```

## Project Structure

```
mini-file-system/
├── include/
│   ├── types.h              # Superblock, Inode, DirEntry, constants (POD only)
│   ├── istorage.h           # IStorage abstraction (Dependency Inversion)
│   ├── disk.h                # Disk : IStorage (real file-backed storage)
│   ├── block_allocator.h     # Free block tracking
│   ├── inode_table.h         # Inode allocation & storage
│   ├── directory.h            # Directory entry management
│   ├── file_data.h             # Byte <-> block mapping for file content
│   ├── interfaces.h            # INavigator / IFileOperations (Interface Segregation)
│   ├── filesystem.h             # Orchestrator, composes the above
│   └── command.h                 # ICommand + CommandRegistry (Open/Closed)
├── src/
│   ├── disk.cpp
│   ├── block_allocator.cpp
│   ├── inode_table.cpp
│   ├── directory.cpp
│   ├── file_data.cpp
│   ├── filesystem_core.cpp        # format/mount/unmount
│   ├── filesystem_ops.cpp          # mkdir/cd/ls/cat/rm/etc, delegates to above
│   ├── commands.cpp                 # concrete ICommand classes
│   └── main.cpp                      # wiring + CLI loop only
├── Makefile
├── build.sh
└── README.md
```

## Known Limitations

Named directly rather than left for someone else to discover:

- **`FileSystem` is still a coordinator with a fairly wide public surface** — splitting `INavigator`/`IFileOperations` helps, but `FileSystem` itself still implements both.
- **No unit tests yet** — the `IStorage` abstraction *enables* fast in-memory testing, but an actual `MemoryStorage` implementation and test suite hasn't been written.
- **No journaling** — a crash mid-write can leave the bitmap and inode table out of sync.
- **Single-indirect only** — caps file size at ~71.5 KB; double/triple indirect pointers would remove that ceiling.
- **No concurrency control** — single-process, single-user by design.
- **Directories can't grow past 12 blocks of entries** — files got an indirect pointer, directories didn't.

## Possible Extensions

- `MemoryStorage` implementation + unit test suite
- Double/triple-indirect block pointers for larger files
- Write-ahead log for crash consistency
- File permissions and timestamps
- Symbolic links

---

<div align="center">

Built as a from-scratch exploration of what's actually happening below `open()`, `read()`, and `write()`.

</div>
