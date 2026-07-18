#!/bin/bash
# Simple build script - no 'make' required.
# Usage: ./build.sh

set -e

echo "Compiling mini file system..."
g++ -std=c++17 -Wall -Wextra -O2 -Iinclude -o minifs \
    src/main.cpp src/disk.cpp src/fs_core.cpp src/fs_dir.cpp src/fs_data.cpp src/fs_ops.cpp

echo "Build successful. Launching..."
./minifs
