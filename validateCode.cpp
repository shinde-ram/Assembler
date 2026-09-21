#include <iostream>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <string>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include "lib.h"

using namespace std;
unordered_map<string, Symbol> symbolTable;

// Initialize opcode table
void initDS(unordered_map<char, unordered_map<string, vector<Encoding>>> &ds)
{
    // Using opcode file as a input
    FILE *opcodeFile = fopen("optable.txt", "r");
    if (opcodeFile == NULL)
    {
        perror("opcodeFile does not exist");
        exit(1);
    }

    char line[256];
    while (fgets(line, sizeof(line), opcodeFile))
    {
        stringstream words(line);
        string mnemonic;
        Encoding enc;

        words >> mnemonic;
        words >> enc.opcode;
        words >> enc.operandCount;
        words >> enc.operand1;
        words >> enc.operand2;
        words >> enc.modRM;
        words >> enc.sib;
        words >> enc.displacement;
        words >> enc.immediate;
        words >> enc.encodingRule;

        mnemonic = toUpper(mnemonic);
        ds[mnemonic[0]][mnemonic].push_back(enc);
    }
    fclose(opcodeFile);
}

// Match the operand with expected one
bool operandMatches(string operand, string expected)
{
    operand = toUpper(operand);

    // Exact match
    if (operand == expected)
        return true;

    // REG32
    if (expected == "REG32")
    {
        if (isReg(operand))
        {
            return true;
        }
    }

    // RM32
    // r/m32 = register OR memory
    if (expected == "RM32")
    {
        // Register
        if (isReg(operand))
        {
            return true;
        }

        // Memory
        if (operand.length() >= 2 && operand[0] == '[' && operand[operand.length() - 1] == ']')
        {
            return true;
        }
    }

    // Immediate
    string type = getOperandType(operand);
    if (expected == "IMM8" && type == "IMM8")
        return true;

    if (expected == "IMM16" && (type == "IMM8" || type == "IMM16"))
        return true;

    if (expected == "IMM32" && (type == "IMM8" || type == "IMM16" || type == "IMM32"))
        return true;

    return false;
}

// Check whether an Encoding matches given operands
bool matchEncoding(Encoding &enc, vector<string> &operands)
{
    if (enc.operandCount != operands.size())
        return false;

    if (enc.operandCount == 0)
        return true;

    if (enc.operandCount >= 1)
    {
        if (!operandMatches(operands[0], enc.operand1))
            return false;
    }

    if (enc.operandCount == 2)
    {
        if (!operandMatches(operands[1], enc.operand2))
            return false;
    }

    return true;
}

void printSymbolTable()
{
    cout << "\n================ SYMBOL TABLE ================\n";

    cout << "Symbol Name\tLocation\tSize\tSection\tValue\n";
    cout << "------------------------------------------------\n";

    for (auto &entry : symbolTable)
    {
        Symbol &symbol = entry.second;

        cout << symbol.name << "\t\t"
             << symbol.location << "\t\t"
             << symbol.size << "\t"
             << symbol.section << "\t"
             << symbol.value << endl;
    }

    cout << "================================================\n";
}

int getCurrentLocation(string section,
                       int textLocation,
                       int dataLocation,
                       int bssLocation)
{
    if (section == ".text")
        return textLocation;

    if (section == ".data")
        return dataLocation;

    if (section == ".bss")
        return bssLocation;

    return 0;
}

void addSymbol(string name,
               int location,
               int size,
               char section,
               string value)
{
    Symbol symbol;

    symbol.name = toUpper(name);
    symbol.location = location;
    symbol.size = size;
    symbol.section = section;
    symbol.value = value;

    symbolTable[symbol.name] = symbol;
}

bool validateData(string line, int &dataLocation)
{
    int i = 0;

    // Skip spaces
    while (line[i] == ' ' || line[i] == '\t')
        i++;

    // Read symbol name
    if (!isalpha(line[i]))
        return false;

    string symbolName;
    while (isalnum(line[i]) || line[i] == '_')
    {
        symbolName += line[i];
        i++;
    }

    // Skip spaces
    while (line[i] == ' ' || line[i] == '\t')
        i++;

    // Read directive
    string directive;
    while (isalpha(line[i]))
    {
        directive += line[i];
        i++;
    }

    directive = toUpper(directive);
    if (directive != "DB" && directive != "DW" && directive != "DD")
    {
        cout << "Error: Invalid data directive " << directive << endl;
        return false;
    }

    // Skip spaces
    while (line[i] == ' ' || line[i] == '\t')
        i++;

    // Read value
    if (line[i] == '\0' || line[i] == '\n')
    {
        cout << "Error: Missing value for " << symbolName << endl;
        return false;
    }

    string value;
    while (line[i] != '\n' && line[i] != '\0')
    {
        if (line[i] != ' ' && line[i] != '\t')
            value += line[i];
        i++;
    }

    if (value.empty())
    {
        cout << "Error: Missing value for " << symbolName << endl;
        return false;
    }

    // Determine size
    int size = 0;

    if (directive == "DB")
        size = 1;
    else if (directive == "DW")
        size = 2;
    else if (directive == "DD")
        size = 4;

    // Add symbol
    addSymbol(symbolName, dataLocation, size, 'd', value);

    // Update location
    dataLocation += size;
    return true;
}

bool validateBss(string line, int &bssLocation)
{
    int i = 0;

    // Read symbol
    while (line[i] == ' ' || line[i] == '\t')
        i++;

    if (!isalpha(line[i]))
        return false;

    string symbolName;
    while (isalnum(line[i]) || line[i] == '_')
    {
        symbolName += line[i];
        i++;
    }

    // Skip spaces
    while (line[i] == ' ' || line[i] == '\t')
        i++;

    // Read directive
    string directive;
    while (isalpha(line[i]))
    {
        directive += line[i];
        i++;
    }

    directive = toUpper(directive);
    if (directive != "RESB" && directive != "RESW" && directive != "RESD")
    {
        cout << "Error: Invalid BSS directive " << directive << endl;
        return false;
    }

    // Skip spaces
    while (line[i] == ' ' || line[i] == '\t')
        i++;

    // Read count
    string value;
    while (isdigit(line[i]))
    {
        value += line[i];
        i++;
    }

    if (value.empty())
    {
        cout << "Error: Missing size for " << symbolName << endl;
        return false;
    }

    int count = stoi(value);
    int elementSize = 0;

    if (directive == "RESB")
        elementSize = 1;
    else if (directive == "RESW")
        elementSize = 2;
    else if (directive == "RESD")
        elementSize = 4;

    int totalSize = count * elementSize;

    // Add symbol
    addSymbol(symbolName, bssLocation, totalSize, 'b', value);

    // Update location
    bssLocation += totalSize;
    return true;
}

bool handleGlobal(string line, string &globalSymbol)
{
    int i = 0;
    while (line[i] == ' ' || line[i] == '\t')
        i++;

    string directive;
    while (isalpha(line[i]))
    {
        directive += line[i];
        i++;
    }

    directive = toUpper(directive);
    if (directive != "GLOBAL")
        return false;

    while (line[i] == ' ' || line[i] == '\t')
        i++;

    string symbol;
    while (isalnum(line[i]) || line[i] == '_')
    {
        symbol += line[i];
        i++;
    }

    if (symbol.empty())
    {
        cout << "Error: Missing symbol after GLOBAL\n";
        return true;
    }

    globalSymbol = toUpper(symbol);
    cout << "Global symbol: " << globalSymbol << endl;
    return true;
}

// Validate source file
void validate(FILE *fp, unordered_map<char, unordered_map<string, vector<Encoding>>> &ds)
{
    char line[256];

    int textLocation = 0;
    int dataLocation = 0;
    int bssLocation = 0;

    string currentSection = "";
    string globalSymbol = "";

    while (fgets(line, sizeof(line), fp))
    {
        int i = 0;

        // Skip leading spaces
        while (line[i] == ' ' || line[i] == '\t')
            i++;

        // Empty line
        if (line[i] == '\0' || line[i] == '\n')
            continue;

        // GLOBAL
        if (handleGlobal(line, globalSymbol))
            continue;

        // SECTION
        if (isalpha(line[i]))
        {
            int j = i;
            string firstWord;

            while (isalpha(line[j]))
            {
                firstWord += line[j];
                j++;
            }

            firstWord = toUpper(firstWord);
            if (firstWord == "SECTION" || firstWord == "section")
            {
                // Skip spaces
                while (line[j] == ' ' || line[j] == '\t')
                    j++;

                string section;
                while (line[j] != ' ' && line[j] != '\t' && line[j] != '\n' && line[j] != '\0')
                {
                    section += line[j];
                    j++;
                }

                section = toUpper(section);
                if (section == ".TEXT" || section == ".text")
                {
                    currentSection = ".text";
                    cout << "\nCurrent Section: .text\n";
                }
                else if (section == ".DATA" || section == ".data")
                {
                    currentSection = ".data";
                    cout << "\nCurrent Section: .data\n";
                }
                else if (section == ".BSS" || section == ".bss")
                {
                    currentSection = ".bss";
                    cout << "\nCurrent Section: .bss\n";
                }
                else
                    cout << "Error: Unknown section " << section << endl;
                continue;
            }
        }

        // DATA SECTION
        if (currentSection == ".data")
        {
            if (!validateData(line, dataLocation))
                cout << "Invalid DATA line: " << line << endl;
            continue;
        }

        // BSS SECTION
        if (currentSection == ".bss")
        {
            if (!validateBss(line, bssLocation))
            {
                cout << "Invalid BSS line: " << line << endl;
            }
            continue;
        }

        // TEXT SECTION
        if (currentSection != ".text")
        {
            cout << "Error: Instruction outside .text section: " << line << endl;
            continue;
        }

        // DETECT LABEL
        string pendingSymbol = "";
        int symbolLocation = 0;

        if (isalpha(line[i]))
        {
            int j = i;
            string symbolName;

            while (isalnum(line[j]) || line[j] == '_')
            {
                symbolName += line[j];
                j++;
            }

            // Label found
            if (line[j] == ':')
            {
                pendingSymbol = toUpper(symbolName);

                // Save location BEFORE instruction
                symbolLocation = textLocation;

                // Move after ':'
                i = j + 1;

                // Skip spaces
                while (line[i] == ' ' || line[i] == '\t')
                    i++;

                // Label-only line
                if (line[i] == '\0' || line[i] == '\n')
                {
                    char sectionType;
                    if (pendingSymbol == globalSymbol)
                        sectionType = 'T';
                    else
                        sectionType = 't';

                    addSymbol(pendingSymbol, symbolLocation, 0, sectionType, "-");
                    continue;
                }
            }
        }

        // FIRST CHARACTER MUST BE ALPHABET
        if (!isalpha(line[i]))
        {
            cout << "Error: Invalid beginning of line: " << line;
            continue;
        }

        // READ MNEMONIC
        string mnemonic;
        while (isalpha(line[i]))
        {
            mnemonic += line[i];
            i++;
        }

        mnemonic = toUpper(mnemonic);

        // FIND MNEMONIC IN DS
        auto typeIt = ds.find(mnemonic[0]);
        if (typeIt == ds.end())
        {
            cout << "Error: Unknown mnemonic " << mnemonic << endl;
            // Do NOT add pendingSymbol.
            continue;
        }

        auto mnemonicIt = typeIt->second.find(mnemonic);
        if (mnemonicIt == typeIt->second.end())
        {
            cout << "Error: Unknown mnemonic " << mnemonic << endl;
            // Do NOT add pendingSymbol.
            continue;
        }

        // Save address of encodings vector
        vector<Encoding> &encodings = mnemonicIt->second;

        // SKIP SPACES AFTER MNEMONIC
        while (line[i] == ' ' || line[i] == '\t')
            i++;

        // EXTRACT OPERANDS
        vector<string> operands;
        bool syntaxError = false;

        // Operands exist
        if (line[i] != '\0' && line[i] != '\n')
        {
            while (true)
            {
                // Skip spaces
                while (line[i] == ' ' || line[i] == '\t')
                    i++;

                // Operand cannot be empty
                if (line[i] == '\0' || line[i] == '\n')
                {
                    syntaxError = true;
                    break;
                }

                // Comma cannot appear where operand should start
                if (line[i] == ',')
                {
                    cout << "Error: Unexpected comma in line: " << line << endl;
                    syntaxError = true;
                    break;
                }

                // Read operand
                string operand;
                while (line[i] != ',' && line[i] != ' ' && line[i] != '\t' && line[i] != '\n' && line[i] != '\0')
                {
                    operand += line[i];
                    i++;
                }

                operand = toUpper(operand);
                if (operand.empty())
                {
                    cout << "Error: Invalid operand in line: " << line << endl;
                    syntaxError = true;
                    break;
                }

                operands.push_back(operand);

                // Skip spaces after operand
                while (line[i] == ' ' || line[i] == '\t')
                    i++;

                // End of line
                if (line[i] == '\0' || line[i] == '\n')
                    break;

                // Must be comma
                if (line[i] == ',')
                {
                    i++;

                    // Skip spaces after comma
                    while (line[i] == ' ' || line[i] == '\t')
                        i++;

                    // Comma cannot be last
                    if (line[i] == '\0' || line[i] == '\n')
                    {
                        cout << "Error: Missing operand after comma: " << line << endl;
                        syntaxError = true;
                        break;
                    }
                    continue;
                }

                // Something other than comma
                cout << "Error: Expected comma in line: " << line << endl;
                syntaxError = true;
                break;
            }
        }

        // SYNTAX ERROR
        if (syntaxError)
        {
            // Do not add pending symbol.
            continue;
        }

        // FIND MATCHING ENCODING
        bool matched = false;
        Encoding *selectedEncoding = nullptr;

        for (Encoding &enc : encodings)
        {
            if (matchEncoding(enc, operands))
            {
                matched = true;
                selectedEncoding = &enc;
                break;
            }
        }

        // NO MATCHING ENCODING
        if (!matched)
        {
            cout << "Error: Invalid operands for " << mnemonic << endl;
            cout << "Given operands: ";

            for (string &operand : operands)
            {
                cout << operand << "(" << getOperandType(operand) << ") ";
            }

            cout << endl << endl;
            // Do not add pending symbol.
            continue;
        }

        // VALID INSTRUCTION
        cout << "Valid: " << mnemonic  << " ";
        for (string &operand : operands)
        {
            cout << operand << "(" << getOperandType(operand) << ") ";
        }
        cout << endl;

        // SELECTED ENCODING
        cout << "Encoding: " << selectedEncoding->opcode << endl;

        // LINE IS VALID
        if (!pendingSymbol.empty())
        {
            char sectionType;
            if (pendingSymbol == globalSymbol)
                sectionType = 'T';
            else
                sectionType = 't';

            addSymbol(pendingSymbol, symbolLocation, 0, sectionType, "-");
        }

        // =================================================
        // INSTRUCTION SIZE
        //
        // Add your calculate-size function here later.
        // =================================================

        /*
        int instructionSize =
            calculateInstructionSize(*selectedEncoding, operands);

        textLocation += instructionSize;
        */

        cout << endl;
    }
}
