#include <iostream>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <string>
#include <cstdio>
#include<cctype>
#include <cstdlib>

using namespace std;

// Structure store all info about a opcode
struct Encoding
{
    string opcode;
    int operandCount;

    string operand1;
    string operand2;

    int modRM;
    int sib;
    int displacement;
    int immediate;
    int encodingRule;
};

// Convert string to uppercase
string toUpper(string str)
{
    for (char &c : str)
        c = toupper(c);

    return str;
}

bool isReg(string &operand){
    if (operand == "EAX" || operand == "EBX" || operand == "ECX" || operand == "EDX" || operand == "ESI" || operand == "EDI" || operand == "EBP" || operand == "ESP")
        return true;
    return false;
}


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

// Get operand type
string getOperandType(string operand)
{
    operand = toUpper(operand);

    // 32-bit registers
    if (isReg(operand)){
        return "REG32";
    }

    // CL
    if (operand == "CL")
        return "CL";

    // Memory operand
    if (operand.length() >= 2 && operand[0] == '[' && operand[operand.length() - 1] == ']'){
        return "RM32";
    }

    // Immediate
    bool number = true;
    int start = 0;

    if (operand.length() == 0)
        return "UNKNOWN";

    if (operand[0] == '-' || operand[0] == '+')
        start = 1;

    if (start == operand.length())
        return "UNKNOWN";

    for (int i = start; i < operand.length(); i++){
        if (!isdigit(operand[i])){
            number = false;
            break;
        }
    }

    if (number)
    {
        long long value = stoll(operand);
        if (value >= -128 && value <= 255)
            return "IMM8";

        if (value >= -32768 && value <= 65535)
            return "IMM16";
        return "IMM32";
    }
    return "UNKNOWN";
}

// Match the operand with expected one 
bool operandMatches(string operand, string expected)
{
    operand = toUpper(operand);

    // Exact match
    if (operand == expected)
        return true;

    // REG32
    if (expected == "REG32"){
        if (isReg(operand)){
            return true;
        }
    }

    // RM32
    // r/m32 = register OR memory
    if (expected == "RM32"){
        // Register
        if (isReg(operand)){
            return true;
        }

        // Memory
        if (operand.length() >= 2 && operand[0] == '[' && operand[operand.length() - 1] == ']'){
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

    if (enc.operandCount >= 1){
        if (!operandMatches(operands[0], enc.operand1))
            return false;
    }

    if (enc.operandCount == 2){
        if (!operandMatches(operands[1], enc.operand2))
            return false;
    }

    return true;
}

// Validate source file
void validate(FILE *fp, unordered_map<char, unordered_map<string, vector<Encoding>>> &ds){
    char line[256];

    while (fgets(line, sizeof(line), fp))
    {
        int i = 0;

        // Skip leading spaces
        while (line[i] == ' ' || line[i] == '\t')
            i++;

        // Empty line
        if (line[i] == '\0' || line[i] == '\n')
            continue;

        // First character must be alphabet
        if (!isalpha(line[i])){
            cout << "Error: Invalid beginning of line: " << line;
            continue;
        }

        // Read mnemonic
        string mnemonic;
        while (isalpha(line[i]))
        {
            mnemonic += line[i];
            i++;
        }
        mnemonic = toUpper(mnemonic);

        // Find mnemonic in DS
        auto typeIt = ds.find(mnemonic[0]);
        if (typeIt == ds.end()){
            cout << "Error: Unknown mnemonic " << mnemonic << endl;
            continue;
        }

        auto mnemonicIt = typeIt->second.find(mnemonic);
        if (mnemonicIt == typeIt->second.end()){
            cout << "Error: Unknown mnemonic " << mnemonic << endl;
            continue;
        }

        // Save the address of encodings vector of mnemonic
        vector<Encoding> &encodings = mnemonicIt->second;

        // Skip spaces after mnemonic
        while (line[i] == ' ' || line[i] == '\t')
            i++;

        // Extract operands
        vector<string> operands;
        bool syntaxError = false;

        // Operands exist
        if(line[i] != '\0' && line[i] != '\n')
        {
            while (true)
            {
                // Skip spaces
                while (line[i] == ' ' || line[i] == '\t')
                    i++;

                // Operand cannot be empty
                if (line[i] == '\0' || line[i] == '\n'){
                    syntaxError = true;
                    break;
                }

                // Comma cannot appear where operand should start
                if (line[i] == ','){
                    cout << "Error: Unexpected comma in line: " << line << endl;
                    syntaxError = true;
                    break;
                }

                // Read operand
                string operand;
                while (line[i] != ',' && line[i] != ' ' && line[i] != '\t' && line[i] != '\n' && line[i] != '\0'){
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
                    if (line[i] == '\0' || line[i] == '\n'){
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

        if (syntaxError)
            continue;

        // Find matching encoding
        bool matched = false;
        Encoding *selectedEncoding = nullptr;
        for (Encoding &enc : encodings){
            if (matchEncoding(enc, operands)){
                matched = true;
                selectedEncoding = &enc;
                break;
            }
        }

        // No matching encoding
        if (!matched)
        {
            cout << "Error: Invalid operands for " << mnemonic << endl;
            cout << "Given operands: ";

            for (string &operand : operands){
                cout << operand << "(" << getOperandType(operand) << ") ";
            }
            cout << endl << endl;
            continue;
        }

        // Valid instruction
        cout << "Valid: " << mnemonic << " ";
        for (string &operand : operands){
            cout << operand << "(" << getOperandType(operand) << ") ";
        }
        cout << endl;

        // Selected encoding
        cout << "Encoding: " << selectedEncoding->opcode << endl;
        cout << endl;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cout << "./executable [sample_assembly_file]";
        exit(1);
    }

    // Data structure for opcode table
    unordered_map<char, unordered_map<string, vector<Encoding>>> ds;

    // Initialize opcode table
    initDS(ds);

    // Open source file
    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL)
    {
        perror("File not open");
        exit(1);
    }

    // Validate source
    validate(fp, ds);
    fclose(fp);
    return 0;
}