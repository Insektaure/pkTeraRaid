#include "personal_table.h"
#include <cstdio>

bool PersonalTable::load(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    raw_.resize(size);
    size_t read = fread(raw_.data(), 1, size, f);
    fclose(f);

    if ((long)read != size) return false;

    count_ = (int)(size / ENTRY_SIZE);
    return count_ > 0;
}
