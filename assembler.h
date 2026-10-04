#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <string>
#include <vector>

using namespace std;

struct OpcodeEntry
{
    string mnemonic;
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

extern vector<OpcodeEntry> table;

void toUpperCase(string &str);
bool isRegister(const string &operand);
bool isMemory(const string &operand);
bool isNumber(const string &operand);
long long getNumber(const string &operand);

bool readOperands(const string &text, vector<string> &operands, string &error);
int findMatchingEncoding(const string &mnemonic, const vector<string> &operands);

void calculateModRMForFile(const string &filename);

#endif
