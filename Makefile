CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
SRC = src/main.cpp src/disk.cpp src/fs_core.cpp src/fs_dir.cpp src/fs_data.cpp src/fs_ops.cpp
TARGET = minifs

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) disk.img

run: all
	./$(TARGET)

.PHONY: all clean run
