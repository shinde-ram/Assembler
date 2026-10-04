#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include <cstdlib>
#include <unordered_map>
#include "assembler.h"

using namespace std;
vector<OpcodeEntry> table;

#define MAX_OPERANDS 2

// Convert string to uppercase
void toUpperCase(string &str)
{
    for (char &c : str)
        c = toupper((unsigned char)c);
}

// Check whether operand is a 32-bit register
bool isRegister(const string &operand)
{
    string registers[] = {"EAX","EBX","ECX","EDX","ESI","EDI","EBP","ESP"};

    for (string reg : registers)
        if (operand == reg)
            return true;

    return false;
}

// Check whether operand is CL
bool isCL(const string &operand)
{
    return operand == "CL";
}

// Check whether operand is memory
// Example: [EAX], [EBX+4], [VALUE]
bool isMemory(const string &operand)
{
    return operand.length() >= 2 &&operand[0] == '[' && operand[operand.length() - 1] == ']';
}

// Check whether operand is a number
bool isNumber(const string &operand)
{
    if (operand.empty())
        return false;

    int start = 0;

    if (operand[0] == '-' || operand[0] == '+')
        start = 1;

    if (start == operand.length())
        return false;

    if (start + 2 < operand.length() && operand[start] == '0' &&
        (operand[start + 1] == 'X' || operand[start + 1] == 'x'))
    {
        for (int i = start + 2; i < operand.length(); i++)
            if (!isxdigit((unsigned char)operand[i]))
                return false;

        return true;
    }

    for (int i = start; i < operand.length(); i++)
        if (!isdigit((unsigned char)operand[i]))
            return false;

    return true;
}

// Check whether operand is a symbol
// Example: LOOP, START, VALUE
bool isSymbol(const string &operand)
{
    if (operand.empty() || !isalpha((unsigned char)operand[0]))
        return false;

    for (int i = 1; i < operand.length(); i++)
        if (!isalnum((unsigned char)operand[i]) && operand[i] != '_')
            return false;

    return true;
}

// Get operand type
string getOperandType(const string &operand)
{
    if (isRegister(operand))
        return "REG32";

    if (isCL(operand))
        return "CL";

    if (isMemory(operand))
        return "RM32";

    if (isNumber(operand))
        return "IMM";

    if (isSymbol(operand))
        return "SYMBOL";

    return "UNKNOWN";
}

// Get numeric value
long long getNumber(const string &operand)
{
    return strtoll(operand.c_str(), NULL, 0);
}

// Check IMM8 range
bool fitsIMM8(const string &operand)
{
    if (!isNumber(operand))
        return false;

    long long value = getNumber(operand);
    return value >= -128 && value <= 255;
}

// Check IMM16 range
bool fitsIMM16(const string &operand)
{
    if (!isNumber(operand))
        return false;

    long long value = getNumber(operand);
    return value >= -32768 && value <= 65535;
}

// Check IMM32 range
bool fitsIMM32(const string &operand)
{
    if (!isNumber(operand))
        return false;

    long long value = getNumber(operand);
    return value >= -2147483648LL && value <= 4294967295LL;
}

// Read opcode table
void readOpcodeTable(const string &filename)
{
    ifstream file(filename);

    if (!file)
    {
        cout << "Error: Cannot open " << filename << endl;
        exit(1);
    }

    string line;

    while (getline(file, line))
    {
        stringstream ss(line);
        OpcodeEntry entry;

        if (!(ss >> entry.mnemonic
                  >> entry.opcode
                  >> entry.operandCount
                  >> entry.operand1
                  >> entry.operand2
                  >> entry.modRM
                  >> entry.sib
                  >> entry.displacement
                  >> entry.immediate
                  >> entry.encodingRule))
            continue;

        toUpperCase(entry.mnemonic);
        toUpperCase(entry.operand1);
        toUpperCase(entry.operand2);

        table.push_back(entry);
    }

    file.close();
}

// Check whether mnemonic exists
bool mnemonicExists(const string &mnemonic)
{
    for (const OpcodeEntry &entry : table)
        if (entry.mnemonic == mnemonic)
            return true;

    return false;
}

// Match the operand with expected one
bool operandMatches(const string &operand, const string &expected)
{
    if (operand == expected)
        return true;

    if (expected == "REG32")
        return isRegister(operand);

    if (expected == "RM32")
        return isRegister(operand) || isMemory(operand);

    if (expected == "RM16")
    {
        if (isMemory(operand))
            return true;

        string registers16[] = {"AX","BX","CX","DX","SI","DI","BP","SP"};

        for (string reg : registers16)
            if (operand == reg)
                return true;
    }

    if (expected == "CL")
        return isCL(operand);

    if (expected == "IMM8")
        return fitsIMM8(operand);

    if (expected == "IMM16")
        return fitsIMM16(operand);

    if (expected == "IMM32")
        return fitsIMM32(operand);

    if (expected == "REL8")
        return isSymbol(operand) || fitsIMM8(operand);

    if (expected == "REL32")
        return isSymbol(operand) || fitsIMM32(operand);

    return false;
}

// Find matching opcode encoding
int findMatchingEncoding(const string &mnemonic, const vector<string> &operands)
{
    for (int i = 0; i < table.size(); i++)
    {
        OpcodeEntry &entry = table[i];

        if (entry.mnemonic != mnemonic)
            continue;

        if (entry.operandCount != operands.size())
            continue;

        if (entry.operandCount == 0)
            return i;

        if (!operandMatches(operands[0], entry.operand1))
            continue;

        if (entry.operandCount == 2 &&
            !operandMatches(operands[1], entry.operand2))
            continue;

        return i;
    }

    return -1;
}

// Print operand information
void printOperand(int number, const string &operand, const string &expected)
{
    cout << "Operand " << number << ": " << operand << endl;

    string type = getOperandType(operand);

    if (type == "IMM" && expected == "IMM8")
        type = "Constant";
    else if (type == "IMM" && expected == "IMM16")
        type = "Constant";
    else if (type == "IMM" && expected == "IMM32")
        type = "Constant";
    else if (type == "REG32" || type == "CL")
        type = "Register";
    else if (type == "RM32" && isMemory(operand))
        type = "Memory";
    else if (type == "SYMBOL")
        type = "Symbol";

    cout << "Type: " << type << " (" << expected << ")" << endl;
}

// Read operands from instruction
bool readOperands(const string &text, vector<string> &operands, string &error)
{
    string input = text;

    while (!input.empty() && isspace((unsigned char)input.front()))
        input.erase(0, 1);

    while (!input.empty() && isspace((unsigned char)input.back()))
        input.pop_back();

    if (input.empty())
        return true;

    if (input.find(',') == string::npos)
    {
        stringstream ss(input);
        string first, second;

        ss >> first >> second;

        if (!first.empty() && !second.empty())
        {
            error = "Missing comma between operands";
            return false;
        }
    }

    string current;
    bool memory = false;

    for (char c : input)
    {
        if (c == '[')
            memory = true;

        if (c == ']')
            memory = false;

        if (c == ',' && !memory)
        {
            while (!current.empty() &&
                   isspace((unsigned char)current.front()))
                current.erase(0, 1);

            while (!current.empty() &&
                   isspace((unsigned char)current.back()))
                current.pop_back();

            if (current.empty())
            {
                if (operands.empty())
                    error = "Missing operand before comma";
                else
                    error = "Missing operand between commas";

                return false;
            }

            toUpperCase(current);
            operands.push_back(current);
            current.clear();
        }
        else
        {
            current += c;
        }
    }

    while (!current.empty() &&
           isspace((unsigned char)current.front()))
        current.erase(0, 1);

    while (!current.empty() &&
           isspace((unsigned char)current.back()))
        current.pop_back();

    if (current.empty())
    {
        error = "Missing operand after comma";
        return false;
    }

    toUpperCase(current);
    operands.push_back(current);

    if (operands.size() > MAX_OPERANDS)
    {
        error = "Too many operands";
        return false;
    }

    return true;
}

// Validate assembly file
void validate(const string &filename)
{
    ifstream file(filename);

    if (!file)
    {
        cout << "Error: Cannot open " << filename << endl;
        return;
    }

    string line;

    while (getline(file, line))
    {
        stringstream ss(line);
        string mnemonic;

        ss >> mnemonic;

        if (mnemonic.empty())
            continue;

        toUpperCase(mnemonic);

        cout << "Mnemonic: " << mnemonic << endl;

        if (!mnemonicExists(mnemonic))
        {
            cout << "Status: Not Found" << endl << endl;
            continue;
        }

        cout << "Status: Found" << endl;

        string operandText;
        getline(ss, operandText);

        vector<string> operands;
        string error;

        if (!readOperands(operandText, operands, error))
        {
            cout << "Syntax Error: " << error << endl << endl;
            continue;
        }

        int index = findMatchingEncoding(mnemonic, operands);

        if (index == -1)
        {
            int expectedCount = -1;

            for (const OpcodeEntry &entry : table)
            {
                if (entry.mnemonic == mnemonic)
                {
                    expectedCount = entry.operandCount;
                    break;
                }
            }

            if (expectedCount != operands.size())
            {
                cout << "Syntax Error: " << mnemonic << " expects " << expectedCount << " operand";

                if (expectedCount != 1)
                    cout << "s";

                cout << ", but " << operands.size() << " were given" << endl;
            }
            else
            {
                for (int i = 0; i < operands.size(); i++)
                {
                    cout << "Operand " << i + 1 << ": "<< operands[i] << endl;

                    cout << "Type: " << getOperandType(operands[i]) << endl;
                }

                cout << "Encoding: Not Found" << endl;
            }

            cout << endl;
            continue;
        }

        OpcodeEntry &entry = table[index];

        if (operands.size() >= 1)
            printOperand(1, operands[0], entry.operand1);

        if (operands.size() == 2)
            printOperand(2, operands[1], entry.operand2);

        cout << "Encoding: " << entry.opcode << endl << endl;
    }

    file.close();
}

// Symbol table - Pass 1

struct Symbol
{
    string name;
    int location;
    int size;
    char section;
    string value;
};

unordered_map<string, Symbol> symbolTable;

// Add a new symbol to the symbol table
void addSymbol(string name, int location, int size, char section, string value)
{
    toUpperCase(name);

    Symbol symbol;
    symbol.name = name;
    symbol.location = location;
    symbol.size = size;
    symbol.section = section;
    symbol.value = value;

    symbolTable[name] = symbol;
}

// Read a DB, DW or DD line from the data section
bool processData(string line, int &dataLocation)
{
    stringstream ss(line);
    string name, directive, value;

    ss >> name >> directive;

    if (name.empty() || directive.empty() || !isSymbol(name))
        return false;

    toUpperCase(directive);

    int size = 0;

    if (directive == "DB")
        size = 1;
    else if (directive == "DW")
        size = 2;
    else if (directive == "DD")
        size = 4;
    else
        return false;

    getline(ss, value);

    while (!value.empty() && isspace((unsigned char)value.front()))
        value.erase(value.begin());

    while (!value.empty() && isspace((unsigned char)value.back()))
        value.pop_back();

    if (value.empty())
    {
        cout << "Error: Missing value for " << name << endl;
        return true;
    }

    addSymbol(name, dataLocation, size, 'd', value);
    dataLocation += size;

    return true;
}

// Read a RESB, RESW or RESD line from the BSS section
bool processBss(string line, int &bssLocation)
{
    stringstream ss(line);
    string name, directive;
    int count;

    ss >> name >> directive;

    if (name.empty() || directive.empty() || !isSymbol(name))
        return false;

    toUpperCase(directive);

    int elementSize = 0;

    if (directive == "RESB")
        elementSize = 1;
    else if (directive == "RESW")
        elementSize = 2;
    else if (directive == "RESD")
        elementSize = 4;
    else
        return false;

    if (!(ss >> count) || count <= 0)
    {
        cout << "Error: Invalid size for " << name << endl;
        return true;
    }

    int totalSize = count * elementSize;

    addSymbol(name, bssLocation, totalSize, 'b', to_string(count));
    bssLocation += totalSize;

    return true;
}

// Display all symbols stored in the table
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

// Read the assembly file and build the symbol table
void buildSymbolTable(const string &filename)
{
    ifstream file(filename);

    if (!file)
    {
        cout << "Error: Cannot open " << filename << endl;
        return;
    }

    symbolTable.clear();

    int textLocation = 0;
    int dataLocation = 0;
    int bssLocation = 0;

    string currentSection = "";
    string globalSymbol = "";
    string line;

    while (getline(file, line))
    {
        string temp = line;

        while (!temp.empty() && isspace((unsigned char)temp.front()))
            temp.erase(temp.begin());

        if (temp.empty() || temp[0] == ';')
            continue;

        stringstream ss(line);
        string firstWord;
        ss >> firstWord;

        if (firstWord.empty())
            continue;

        string upperWord = firstWord;
        toUpperCase(upperWord);

        // Read the global symbol name
        if (upperWord == "GLOBAL")
        {
            ss >> globalSymbol;

            if (globalSymbol.empty() || !isSymbol(globalSymbol))
            {
                cout << "Error: Missing symbol after GLOBAL" << endl;
                continue;
            }

            toUpperCase(globalSymbol);
            continue;
        }

        // Change the current section
        if (upperWord == "SECTION")
        {
            string section;
            ss >> section;
            toUpperCase(section);

            if (section == ".TEXT")
                currentSection = ".text";
            else if (section == ".DATA")
                currentSection = ".data";
            else if (section == ".BSS")
                currentSection = ".bss";
            else
                cout << "Error: Unknown section " << section << endl;

            continue;
        }

        // Process variables in the data section
        if (currentSection == ".data")
        {
            if (!processData(line, dataLocation))
                cout << "Invalid DATA line: " << line << endl;

            continue;
        }

        // Process reserved variables in the BSS section
        if (currentSection == ".bss")
        {
            if (!processBss(line, bssLocation))
                cout << "Invalid BSS line: " << line << endl;

            continue;
        }

        // Instructions and labels should be inside .text
        if (currentSection != ".text")
        {
            cout << "Error: Instruction outside .text section: " << line << endl;
            continue;
        }

        string instructionLine = line;
        stringstream labelCheck(instructionLine);
        string first;
        labelCheck >> first;

        string pendingSymbol;
        int symbolLocation = textLocation;

        // Check whether the line starts with a label
        size_t colon = first.find(':');

        if (colon != string::npos)
        {
            pendingSymbol = first.substr(0, colon);

            if (isSymbol(pendingSymbol))
            {
                toUpperCase(pendingSymbol);

                if (colon + 1 < first.length())
                    instructionLine = first.substr(colon + 1) +
                                      line.substr(line.find(first) + first.length());
                else
                    instructionLine = line.substr(line.find(first) + first.length());

                while (!instructionLine.empty() &&
                       isspace((unsigned char)instructionLine.front()))
                    instructionLine.erase(instructionLine.begin());

                // Store a label even when it has no instruction on the same line
                if (instructionLine.empty())
                {
                    char sectionType =
                        (pendingSymbol == globalSymbol) ? 'T' : 't';

                    addSymbol(pendingSymbol, symbolLocation, 0,
                              sectionType, "-");
                    continue;
                }
            }
        }

        // Validate the instruction using the existing opcode logic
        stringstream instructionStream(instructionLine);
        string mnemonic;
        instructionStream >> mnemonic;

        if (mnemonic.empty())
            continue;

        toUpperCase(mnemonic);

        if (!mnemonicExists(mnemonic))
        {
            cout << "Mnemonic: " << mnemonic << endl;
            cout << "Status: Not Found" << endl << endl;
            continue;
        }

        string operandText;
        getline(instructionStream, operandText);

        vector<string> operands;
        string error;

        if (!readOperands(operandText, operands, error))
        {
            cout << "Syntax Error: " << error << endl << endl;
            continue;
        }

        int index = findMatchingEncoding(mnemonic, operands);

        if (index == -1)
        {
            cout << "Mnemonic: " << mnemonic << endl;
            cout << "Status: Found" << endl;
            cout << "Encoding: Not Found" << endl << endl;
            continue;
        }

        OpcodeEntry &entry = table[index];

        cout << "Mnemonic: " << mnemonic << endl;
        cout << "Status: Found" << endl;

        if (operands.size() >= 1)
            printOperand(1, operands[0], entry.operand1);

        if (operands.size() == 2)
            printOperand(2, operands[1], entry.operand2);

        cout << "Encoding: " << entry.opcode << endl << endl;

        // Add the label after the instruction is found to be valid
        if (!pendingSymbol.empty())
        {
            char sectionType =
                (pendingSymbol == globalSymbol) ? 'T' : 't';

            addSymbol(pendingSymbol, symbolLocation, 0,
                      sectionType, "-");
        }
    }

    file.close();
}

// Main
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cout << "Usage: " << argv[0] << " <opcode_file> <assembly_file>" << endl;
        return 1;
    }

    readOpcodeTable(argv[1]);
    buildSymbolTable(argv[2]);
    
    printSymbolTable();
    
    calculateModRMForFile(argv[2]);

    return 0;
}
