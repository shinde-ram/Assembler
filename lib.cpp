
#include "lib.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

unordered_map<string, Symbol> symbolTable;

string toUpper(string s)
{
    transform(
        s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(toupper(c));
        }
    );

    return s;
}


bool isReg(const string &operand)
{
    string reg = toUpper(operand);

    static const vector<string> registers = {
        // 32-bit registers
        "EAX", "EBX", "ECX", "EDX",
        "ESI", "EDI", "EBP", "ESP",

        // 16-bit registers
        "AX", "BX", "CX", "DX",
        "SI", "DI", "BP", "SP",

        // 8-bit registers
        "AL", "AH", "BL", "BH",
        "CL", "CH", "DL", "DH",

        // Segment registers
        "CS", "DS", "ES", "SS", "FS", "GS"
    };

    return find(
        registers.begin(),
        registers.end(),
        reg
    ) != registers.end();
}


string getOperandType(const string &operand)
{
    string op = toUpper(operand);

    // Register operands.
    if (op == "CL")
        return "CL";

    if (op == "EAX")
        return "EAX";

    if (op == "AX")
        return "AX";

    if (op == "AL")
        return "AL";

    if (op == "DX")
        return "DX";

    if (op == "ECX" || op == "EDX" ||
        op == "EBX" || op == "ESI" ||
        op == "EDI" || op == "EBP" ||
        op == "ESP")
    {
        return "REG32";
    }

    if (op == "AX" || op == "BX" ||
        op == "CX" || op == "DX" ||
        op == "SI" || op == "DI" ||
        op == "BP" || op == "SP")
    {
        return "REG16";
    }

    if (op == "AL" || op == "AH" ||
        op == "BL" || op == "BH" ||
        op == "CL" || op == "CH" ||
        op == "DL" || op == "DH")
    {
        return "REG8";
    }

    // Memory operands.
    if (op.find('[') != string::npos &&
        op.find(']') != string::npos)
    {
        return "RM32";
    }

    if (!op.empty())
    {
        char *end = nullptr;
        const char *start = op.c_str();

        // Handle optional sign.
        if (*start == '+' || *start == '-')
            ++start;

        if (*start != '\0')
        {
            (void)strtoll(start, &end, 0);

            if (end != start && *end == '\0')
            {
                long long value = strtoll(
                    op.c_str(),
                    nullptr,
                    0
                );

                if (value >= -128 && value <= 255)
                    return "IMM8";

                if (value >= -32768 && value <= 65535)
                    return "IMM16";

                if (value >= -2147483648LL &&
                    value <= 4294967295LL)
                {
                    return "IMM32";
                }
            }
        }
    }

    // Quoted character or string constants.
    if (op.size() >= 2 &&
        ((op.front() == '\'' && op.back() == '\'') ||
         (op.front() == '"' && op.back() == '"')))
    {
        return "STRING";
    }

    return "UNKNOWN";
}

void initDS(OpcodeTable &ds)
{
    ifstream file("optable.txt");

    if (!file.is_open())
    {
        cerr << "Error: unable to open optable.txt\n";
        return;
    }

    string line;
    int lineNumber = 0;

    while (getline(file, line))
    {
        ++lineNumber;

        // Ignore blank lines and comment lines.
        if (line.empty() || line[0] == '#')
            continue;

        istringstream input(line);

        string mnemonic;
        Encoding encoding{};

        if (!(input >> mnemonic
                     >> encoding.opcode
                     >> encoding.operandCount
                     >> encoding.operand1
                     >> encoding.operand2
                     >> encoding.modRM
                     >> encoding.sib
                     >> encoding.displacement
                     >> encoding.immediate
                     >> encoding.encodingRule))
        {
            cerr << "Warning: invalid opcode-table entry at line "
                 << lineNumber << '\n';
            continue;
        }

        mnemonic = toUpper(mnemonic);

        ds[mnemonic[0]][mnemonic].push_back(encoding);
    }
}

// --------------------------------------------------
// Check whether an actual operand matches the expected
// operand type specified in optable.txt.
// --------------------------------------------------

bool operandMatches(
    const string &actualOperand,
    const string &expectedType)
{
    string actual = toUpper(actualOperand);
    string expected = toUpper(expectedType);

    if (expected == "NONE")
        return actual.empty();

    if (expected == "REG32")
    {
        string type = getOperandType(actual);

        return type == "REG32" ||
               actual == "EAX";
    }

    if (expected == "REG16")
        return getOperandType(actual) == "REG16";

    if (expected == "REG8")
        return getOperandType(actual) == "REG8";

    if (expected == "RM32")
    {
        string type = getOperandType(actual);

        return type == "REG32" ||
               actual == "EAX" ||
               type == "RM32";
    }

    if (expected == "RM16")
    {
        return getOperandType(actual) == "REG16" ||
               (actual.find('[') != string::npos &&
                actual.find(']') != string::npos);
    }

    if (expected == "RM8")
    {
        return getOperandType(actual) == "REG8" ||
               (actual.find('[') != string::npos &&
                actual.find(']') != string::npos);
    }

    if (expected == "CL" ||
        expected == "EAX" ||
        expected == "AX" ||
        expected == "AL" ||
        expected == "DX")
    {
        return actual == expected;
    }

    if (expected == "IMM8" ||
        expected == "IMM16" ||
        expected == "IMM32")
    {
        string type = getOperandType(actual);

        if (type != "IMM8" &&
            type != "IMM16" &&
            type != "IMM32")
        {
            return false;
        }

        return true;
    }

    if (expected == "REL8" || expected == "REL32")
    {
        string type = getOperandType(actual);

        return type == "IMM8" ||
               type == "IMM16" ||
               type == "IMM32" ||
               type == "UNKNOWN";
    }

    if (expected == "STRING")
        return getOperandType(actual) == "STRING";

    return false;
}


const Encoding *matchEncoding(
    const string &mnemonic,
    const vector<string> &operands,
    const OpcodeTable &ds)
{
    string instruction = toUpper(mnemonic);

    auto first = ds.find(instruction[0]);

    if (first == ds.end())
        return nullptr;

    auto second = first->second.find(instruction);

    if (second == first->second.end())
        return nullptr;

    for (const Encoding &encoding : second->second)
    {
        if (static_cast<int>(operands.size()) !=
            encoding.operandCount)
        {
            continue;
        }

        bool matches = true;

        if (encoding.operandCount >= 1)
        {
            matches = operandMatches(
                operands[0],
                encoding.operand1
            );
        }

        if (matches && encoding.operandCount >= 2)
        {
            matches = operandMatches(
                operands[1],
                encoding.operand2
            );
        }

        if (matches)
            return &encoding;
    }

    return nullptr;
}
