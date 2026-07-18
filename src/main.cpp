#include "fs.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>

void printHelp() {
    std::cout <<
        "\nAvailable commands:\n"
        "  mkdir <name>            create a directory\n"
        "  rmdir <name>            remove an empty directory\n"
        "  cd <name|..>            change directory\n"
        "  ls                      list contents of current directory\n"
        "  tree                    show full directory tree\n"
        "  pwd                     print current path\n"
        "  touch <name>            create an empty file\n"
        "  write <name> <text>     overwrite file with text\n"
        "  append <name> <text>    append text to file\n"
        "  cat <name>              print file contents\n"
        "  rm <name>               delete a file\n"
        "  stat <name>             show inode info for file/dir\n"
        "  df                      show disk usage summary\n"
        "  help                    show this message\n"
        "  exit / quit             unmount and exit\n\n";
}

std::vector<std::string> tokenize(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

// join tokens[from..end] back into a single space-separated string
std::string joinFrom(const std::vector<std::string>& tokens, size_t from) {
    std::string result;
    for (size_t i = from; i < tokens.size(); i++) {
        if (i > from) result += " ";
        result += tokens[i];
    }
    return result;
}

int main(int argc, char* argv[]) {
    std::string diskPath = "disk.img";
    if (argc > 1) diskPath = argv[1];

    FileSystem fs;

    std::ifstream test(diskPath);
    bool exists = test.good();
    test.close();

    if (exists) {
        if (!fs.mount(diskPath)) {
            std::cout << "Failed to mount " << diskPath << ". Formatting a fresh disk...\n";
            if (!fs.format(diskPath)) return 1;
        } else {
            std::cout << "Mounted existing disk image: " << diskPath << "\n";
        }
    } else {
        std::cout << "No existing disk image found. Creating a new one...\n";
        if (!fs.format(diskPath)) return 1;
    }

    std::cout << "\n===== Mini File System CLI =====\n";
    std::cout << "Type 'help' for a list of commands.\n\n";

    std::string line;
    while (true) {
        std::cout << "minifs:" << std::flush;
        fs.pwd();
        std::cout << "$ ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::vector<std::string> tokens = tokenize(line);
        std::string cmd = tokens[0];

        if (cmd == "exit" || cmd == "quit") {
            break;
        } else if (cmd == "help") {
            printHelp();
        } else if (cmd == "mkdir") {
            if (tokens.size() < 2) { std::cout << "usage: mkdir <name>\n"; continue; }
            fs.mkdir(tokens[1]);
        } else if (cmd == "rmdir") {
            if (tokens.size() < 2) { std::cout << "usage: rmdir <name>\n"; continue; }
            fs.rmdir(tokens[1]);
        } else if (cmd == "cd") {
            if (tokens.size() < 2) { std::cout << "usage: cd <name|..>\n"; continue; }
            fs.cd(tokens[1]);
        } else if (cmd == "ls") {
            fs.ls();
        } else if (cmd == "tree") {
            fs.tree();
        } else if (cmd == "pwd") {
            fs.pwd();
        } else if (cmd == "touch") {
            if (tokens.size() < 2) { std::cout << "usage: touch <name>\n"; continue; }
            fs.touch(tokens[1]);
        } else if (cmd == "write") {
            if (tokens.size() < 2) { std::cout << "usage: write <name> <text>\n"; continue; }
            fs.writeFile(tokens[1], joinFrom(tokens, 2), false);
        } else if (cmd == "append") {
            if (tokens.size() < 2) { std::cout << "usage: append <name> <text>\n"; continue; }
            fs.writeFile(tokens[1], joinFrom(tokens, 2), true);
        } else if (cmd == "cat") {
            if (tokens.size() < 2) { std::cout << "usage: cat <name>\n"; continue; }
            fs.catFile(tokens[1]);
        } else if (cmd == "rm") {
            if (tokens.size() < 2) { std::cout << "usage: rm <name>\n"; continue; }
            fs.rm(tokens[1]);
        } else if (cmd == "stat") {
            if (tokens.size() < 2) { std::cout << "usage: stat <name>\n"; continue; }
            fs.stat(tokens[1]);
        } else if (cmd == "df") {
            fs.dfInfo();
        } else {
            std::cout << "Unknown command: " << cmd << " (type 'help')\n";
        }
    }

    fs.unmount();
    std::cout << "Unmounted. Goodbye!\n";
    return 0;
}
