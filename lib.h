#include <iostream>
#include <string>

using namespace std;

#ifndef LIB_H
#define LIB_H

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

unsigned char calculateModRM(const Encoding &enc, string operand1, string operand2);

string toUpper(string str);

bool isReg(string &operand);

string getOperandType(string operand);

void modRM(unordered_map<char, unordered_map<string, vector<Encoding>>> &ds, FILE *fp);

void initDS(unordered_map<char, unordered_map<string, vector<Encoding>>> &ds);

bool operandMatches(string operand, string expected);

bool matchEncoding(Encoding &enc, vector<string> &operands);

void validate(FILE *fp, unordered_map<char, unordered_map<string, vector<Encoding>>> &ds);

#endif