#include "elfWriter.h"

#include <elf.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

static uint32_t alignTo(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

bool writeELF32Object(
    const string &filename,
    const vector<uint8_t> &textSection,
    const vector<ELF32Symbol> &symbols
)
{
    static_assert(sizeof(Elf32_Ehdr) == 52);
    static_assert(sizeof(Elf32_Shdr) == 40);
    static_assert(sizeof(Elf32_Sym) == 16);

    // Section indices
    constexpr int TEXT_INDEX = 1;
    constexpr int SYMTAB_INDEX = 2;
    constexpr int STRTAB_INDEX = 3;
    constexpr int SHSTRTAB_INDEX = 4;

    // ------------------------------------------------
    // 1. Prepare symbol string table (.strtab)
    // ------------------------------------------------

    vector<char> strtab;
    strtab.push_back('\0');

    struct SymbolEntry {
        Elf32_Sym elfSymbol;
        bool isGlobal;
    };

    vector<SymbolEntry> symbolEntries;

    // ELF requires the first symbol to be NULL.
    SymbolEntry nullSymbol = {};
    symbolEntries.push_back(nullSymbol);

    // Local symbols must appear before global symbols.
    vector<ELF32Symbol> sortedSymbols = symbols;

    stable_sort(
        sortedSymbols.begin(),
        sortedSymbols.end(),
        [](const ELF32Symbol &a, const ELF32Symbol &b) {
            return !a.isGlobal && b.isGlobal;
        }
    );

    uint32_t firstGlobal = 1;

    for (const auto &symbol : sortedSymbols) {
        Elf32_Sym entry = {};

        entry.st_name = strtab.size();

        strtab.insert(
            strtab.end(),
            symbol.name.begin(),
            symbol.name.end()
        );
        strtab.push_back('\0');

        entry.st_value = symbol.location;
        entry.st_size = symbol.size;

        unsigned char binding =
            symbol.isGlobal ? STB_GLOBAL : STB_LOCAL;

        entry.st_info = ELF32_ST_INFO(binding, STT_NOTYPE);
        entry.st_other = STV_DEFAULT;
        entry.st_shndx = TEXT_INDEX;

        symbolEntries.push_back({entry, symbol.isGlobal});

        if (symbol.isGlobal)
            firstGlobal = symbolEntries.size() - 1;
    }

    // If there are no global symbols, sh_info is the total
    // number of symbols.
    if (firstGlobal == 1 && sortedSymbols.size() > 0) {
        bool hasGlobal = false;

        for (const auto &symbol : sortedSymbols) {
            if (symbol.isGlobal) {
                hasGlobal = true;
                break;
            }
        }

        if (!hasGlobal)
            firstGlobal = symbolEntries.size();
    }

    // ------------------------------------------------
    // 2. Prepare section-name string table
    // ------------------------------------------------

    const char shstrtab[] =
        "\0.text\0.symtab\0.strtab\0.shstrtab\0";

    // ------------------------------------------------
    // 3. Calculate file offsets
    // ------------------------------------------------

    Elf32_Ehdr ehdr = {};

    memcpy(ehdr.e_ident, ELFMAG, SELFMAG);
    ehdr.e_ident[EI_CLASS] = ELFCLASS32;
    ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_ident[EI_OSABI] = ELFOSABI_NONE;

    ehdr.e_type = ET_REL;
    ehdr.e_machine = EM_386;
    ehdr.e_version = EV_CURRENT;

    ehdr.e_ehsize = sizeof(Elf32_Ehdr);
    ehdr.e_shentsize = sizeof(Elf32_Shdr);
    ehdr.e_shnum = 5;
    ehdr.e_shstrndx = SHSTRTAB_INDEX;

    uint32_t textOffset = sizeof(Elf32_Ehdr);

    uint32_t symtabOffset =
        alignTo(textOffset + textSection.size(), 4);

    uint32_t symtabSize =
        symbolEntries.size() * sizeof(Elf32_Sym);

    uint32_t strtabOffset = symtabOffset + symtabSize;

    uint32_t strtabSize = strtab.size();

    uint32_t shstrtabOffset = strtabOffset + strtabSize;

    uint32_t shstrtabSize = sizeof(shstrtab);

    uint32_t sectionHeaderOffset =
        alignTo(shstrtabOffset + shstrtabSize, 4);

    ehdr.e_shoff = sectionHeaderOffset;

    // ------------------------------------------------
    // 4. Build section headers
    // ------------------------------------------------

    Elf32_Shdr sectionHeaders[5] = {};

    // Section 0: NULL
    // Already zero initialized.

    // Section 1: .text
    sectionHeaders[TEXT_INDEX].sh_name = 1;
    sectionHeaders[TEXT_INDEX].sh_type = SHT_PROGBITS;
    sectionHeaders[TEXT_INDEX].sh_flags =
        SHF_ALLOC | SHF_EXECINSTR;
    sectionHeaders[TEXT_INDEX].sh_offset = textOffset;
    sectionHeaders[TEXT_INDEX].sh_size = textSection.size();
    sectionHeaders[TEXT_INDEX].sh_addralign = 1;

    // Section 2: .symtab
    sectionHeaders[SYMTAB_INDEX].sh_name = 7;
    sectionHeaders[SYMTAB_INDEX].sh_type = SHT_SYMTAB;
    sectionHeaders[SYMTAB_INDEX].sh_offset = symtabOffset;
    sectionHeaders[SYMTAB_INDEX].sh_size = symtabSize;
    sectionHeaders[SYMTAB_INDEX].sh_link = STRTAB_INDEX;
    sectionHeaders[SYMTAB_INDEX].sh_info = firstGlobal;
    sectionHeaders[SYMTAB_INDEX].sh_addralign = 4;
    sectionHeaders[SYMTAB_INDEX].sh_entsize = sizeof(Elf32_Sym);

    // Section 3: .strtab
    sectionHeaders[STRTAB_INDEX].sh_name = 15;
    sectionHeaders[STRTAB_INDEX].sh_type = SHT_STRTAB;
    sectionHeaders[STRTAB_INDEX].sh_offset = strtabOffset;
    sectionHeaders[STRTAB_INDEX].sh_size = strtabSize;
    sectionHeaders[STRTAB_INDEX].sh_addralign = 1;

    // Section 4: .shstrtab
    sectionHeaders[SHSTRTAB_INDEX].sh_name = 23;
    sectionHeaders[SHSTRTAB_INDEX].sh_type = SHT_STRTAB;
    sectionHeaders[SHSTRTAB_INDEX].sh_offset = shstrtabOffset;
    sectionHeaders[SHSTRTAB_INDEX].sh_size = shstrtabSize;
    sectionHeaders[SHSTRTAB_INDEX].sh_addralign = 1;

    // ------------------------------------------------
    // 5. Write ELF file
    // ------------------------------------------------

    ofstream out(filename, ios::binary);

    if (!out) {
        cerr << "Unable to create object file\n";
        return false;
    }

    out.write(
        reinterpret_cast<const char *>(&ehdr),
        sizeof(ehdr)
    );

    if (!textSection.empty()) {
        out.write(
            reinterpret_cast<const char *>(textSection.data()),
            textSection.size()
        );
    }

    // Alignment before .symtab
    while (static_cast<uint32_t>(out.tellp()) < symtabOffset)
        out.put('\0');

    for (const auto &entry : symbolEntries) {
        out.write(
            reinterpret_cast<const char *>(&entry.elfSymbol),
            sizeof(Elf32_Sym)
        );
    }

    out.write(strtab.data(), strtab.size());
    out.write(shstrtab, shstrtabSize);

    while (static_cast<uint32_t>(out.tellp()) < sectionHeaderOffset)
        out.put('\0');

    out.write(
        reinterpret_cast<const char *>(sectionHeaders),
        sizeof(sectionHeaders)
    );

    out.close();

    if (!out) {
        cerr << "Error while writing ELF object file\n";
        return false;
    }

    cout << "ELF32 object file generated: " << filename << '\n';

    return true;
}