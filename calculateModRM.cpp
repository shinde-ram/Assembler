#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include "assembler.h"

using namespace std;

// Return the 3-bit code used for a register
int getRegisterCode(const string &reg)
{
    if (reg == "EAX") return 0;
    if (reg == "ECX") return 1;
    if (reg == "EDX") return 2;
    if (reg == "EBX") return 3;
    if (reg == "ESP") return 4;
    if (reg == "EBP") return 5;
    if (reg == "ESI") return 6;
    if (reg == "EDI") return 7;

    return -1;
}

// Remove brackets and spaces from a memory operand
string getAddress(const string &operand)
{
    string address = operand.substr(1, operand.length() - 2);
    string result;

    for (char c : address)
    {
        if (!isspace((unsigned char)c))
            result += c;
    }

    return result;
}

// Find the register used as the base register
string getBaseRegister(const string &operand)
{
    string address = getAddress(operand);
    size_t star = address.find('*');

    // In a scaled address, first find the index register.
    string indexRegister = "";

    if (star != string::npos)
    {
        string beforeStar = address.substr(0, star);
        string registers[] = {"EAX", "ECX", "EDX", "EBX",
                              "ESP", "EBP", "ESI", "EDI"};

        int lastPosition = -1;

        for (string reg : registers)
        {
            size_t position = beforeStar.rfind(reg);

            if (position != string::npos && (int)position > lastPosition)
            {
                indexRegister = reg;
                lastPosition = (int)position;
            }
        }
    }

    string registers[] = {"EAX", "ECX", "EDX", "EBX",
                          "ESP", "EBP", "ESI", "EDI"};

    for (string reg : registers)
    {
        if (reg == indexRegister)
            continue;

        if (address.find(reg) != string::npos)
            return reg;
    }

    return "";
}

// Find the index register in a scaled address
string getIndexRegister(const string &operand)
{
    string address = getAddress(operand);
    size_t star = address.find('*');

    if (star == string::npos)
        return "";

    string beforeStar = address.substr(0, star);

    string registers[] = {"EAX", "ECX", "EDX", "EBX",
                          "ESP", "EBP", "ESI", "EDI"};

    string indexRegister = "";
    int lastPosition = -1;

    // The register closest to * is the index register.
    for (string reg : registers)
    {
        size_t position = beforeStar.rfind(reg);

        if (position != string::npos && (int)position > lastPosition)
        {
            indexRegister = reg;
            lastPosition = (int)position;
        }
    }

    return indexRegister;
}

// Find a displacement such as +4 or -8
bool getDisplacement(const string &operand, long long &value)
{
    string address = getAddress(operand);
    value = 0;

    string part;
    char sign = '+';
    bool found = false;

    for (size_t i = 0; i <= address.length(); i++)
    {
        if (i == address.length() || address[i] == '+' || address[i] == '-')
        {
            if (!part.empty() && isNumber(part))
            {
                long long number = getNumber(part);

                if (sign == '-')
                    number = -number;

                value += number;
                found = true;
            }

            if (i < address.length())
                sign = address[i];

            part.clear();
        }
        else
        {
            part += address[i];
        }
    }

    return found;
}

// Calculate the MOD field
int getMod(const string &operand)
{
    if (isRegister(operand))
        return 3;                       // 11 for register

    if (!isMemory(operand))
        return -1;

    string base = getBaseRegister(operand);
    long long displacement;

    // No displacement means MOD 00 in normal cases.
    if (!getDisplacement(operand, displacement))
    {
        // EBP cannot use MOD 00 without a displacement.
        if (base == "EBP")
            return 1;

        return 0;
    }

    // Small displacement fits in 8 bits.
    if (displacement >= -128 && displacement <= 127)
        return 1;

    // Larger displacement uses 32 bits.
    return 2;
}

// Calculate the R/M field
int getRM(const string &operand)
{
    if (isRegister(operand))
        return getRegisterCode(operand);

    if (!isMemory(operand))
        return -1;

    string address = getAddress(operand);

    // R/M 100 means that a SIB byte follows.
    if (address.find('*') != string::npos)
        return 4;

    string base = getBaseRegister(operand);

    // No base register means displacement-only addressing.
    if (base.empty())
        return 5;

    return getRegisterCode(base);
}

// Find which operand is stored in the R/M field
int getRMOperand(const OpcodeEntry &entry)
{
    if (entry.operand1 == "RM32" || entry.operand1 == "RM16")
        return 0;

    if (entry.operand2 == "RM32" || entry.operand2 == "RM16")
        return 1;

    return -1;
}

// Calculate the complete ModR/M byte
int calculateModRM(const OpcodeEntry &entry, const vector<string> &operands)
{
    int rmOperand = getRMOperand(entry);

    if (rmOperand == -1 || rmOperand >= (int)operands.size())
        return -1;

    int reg = 0;

    // encodingRule 1 means /r.
    if (entry.encodingRule == 1)
    {
        int regOperand;

        if (rmOperand == 0)
            regOperand = 1;
        else
            regOperand = 0;

        if (regOperand >= (int)operands.size())
            return -1;

        reg = getRegisterCode(operands[regOperand]);

        if (reg == -1)
            return -1;
    }
    // encodingRule 2 to 9 means /0 to /7.
    else if (entry.encodingRule >= 2 && entry.encodingRule <= 9)
    {
        reg = entry.encodingRule - 2;
    }
    else
    {
        return -1;
    }

    int mod = getMod(operands[rmOperand]);
    int rm = getRM(operands[rmOperand]);

    if (mod == -1 || rm == -1)
        return -1;

    // Put MOD, REG and R/M in their correct positions.
    int modrm = (mod << 6) | (reg << 3) | rm;

    return modrm;
}

// Check whether this memory operand needs a SIB byte
bool needsSIB(const string &operand)
{
    if (!isMemory(operand))
        return false;

    string address = getAddress(operand);

    // A scaled index always needs SIB.
    if (address.find('*') != string::npos)
        return true;

    // ESP as a base is also encoded using SIB.
    if (getBaseRegister(operand) == "ESP")
        return true;

    return false;
}

// Return the scale bits used in the SIB byte
int getScale(const string &operand)
{
    string address = getAddress(operand);
    size_t star = address.find('*');

    if (star == string::npos)
        return 0;       // scale 00 means x1

    string scale = address.substr(star + 1);

    if (scale == "2")
        return 1;       // 01 means x2

    if (scale == "4")
        return 2;       // 10 means x4

    if (scale == "8")
        return 3;       // 11 means x8

    return 0;
}

// Calculate the SIB byte
int calculateSIB(const string &operand)
{
    int scale = getScale(operand);
    int index = 4;       // 100 means no index
    int base = 5;        // 101 is used when there is no base

    string indexRegister = getIndexRegister(operand);

    if (!indexRegister.empty())
        index = getRegisterCode(indexRegister);

    string baseRegister = getBaseRegister(operand);

    if (!baseRegister.empty())
        base = getRegisterCode(baseRegister);

    // Put Scale, Index and Base into one byte.
    int sib = (scale << 6) | (index << 3) | base;

    return sib;
}

// Read the .text section and calculate ModR/M and SIB
void calculateModRMForFile(const string &filename)
{
    ifstream file(filename);

    if (!file)
    {
        cout << "Error: Cannot open " << filename << endl;
        return;
    }

    string line;
    string currentSection;

    cout << "\n================ MODR/M CALCULATION ================\n";

    while (getline(file, line))
    {
        stringstream ss(line);
        string firstWord;
        ss >> firstWord;

        if (firstWord.empty() || firstWord[0] == ';')
            continue;

        string upperWord = firstWord;
        toUpperCase(upperWord);

        if (upperWord == "GLOBAL")
            continue;

        if (upperWord == "SECTION")
        {
            ss >> currentSection;
            toUpperCase(currentSection);
            continue;
        }

        if (currentSection != ".TEXT")
            continue;

        string instructionLine = line;

        // Remove a label if it is present before the instruction.
        size_t colon = instructionLine.find(':');

        if (colon != string::npos)
            instructionLine = instructionLine.substr(colon + 1);

        while (!instructionLine.empty() &&
               isspace((unsigned char)instructionLine.front()))
        {
            instructionLine.erase(instructionLine.begin());
        }

        if (instructionLine.empty())
            continue;

        stringstream instructionStream(instructionLine);
        string mnemonic;
        instructionStream >> mnemonic;
        toUpperCase(mnemonic);

        string operandText;
        getline(instructionStream, operandText);

        vector<string> operands;
        string error;

        // Use the same operand parser from the old program.
        if (!readOperands(operandText, operands, error))
            continue;

        // Use the old opcode matching code to find the correct encoding.
        int index = findMatchingEncoding(mnemonic, operands);

        if (index == -1)
            continue;

        OpcodeEntry &entry = table[index];

        if (entry.modRM == 0)
            continue;

        int modrm = calculateModRM(entry, operands);

        if (modrm == -1)
            continue;

        cout << "Instruction: " << mnemonic << endl;
        cout << "Opcode: " << entry.opcode << endl;
        cout << "ModR/M: " << modrm << endl;

        int rmOperand = getRMOperand(entry);

        if (rmOperand != -1 &&
            entry.sib == 1 &&
            needsSIB(operands[rmOperand]))
        {
            cout << "SIB: " << calculateSIB(operands[rmOperand]) << endl;
        }

        cout << endl;
    }

    file.close();
}
