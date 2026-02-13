#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <utility>

// Simple line-based text file loader for name lists
namespace TextData {

inline std::vector<std::string> loadLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream f(path);
    if (!f.is_open()) return lines;
    std::string line;
    while (std::getline(f, line)) {
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(std::move(line));
    }
    return lines;
}

} // namespace TextData
