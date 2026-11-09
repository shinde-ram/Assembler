#ifndef ELF_WRITER_H
#define ELF_WRITER_H

#include <cstdint>
#include <string>
#include <vector>

struct ELF32Symbol {
    std::string name;
    uint32_t location;
    uint32_t size;
    bool isGlobal;
};

bool writeELF32Object(
    const std::string &filename,
    const std::vector<uint8_t> &textSection,
    const std::vector<ELF32Symbol> &symbols
);

#endif