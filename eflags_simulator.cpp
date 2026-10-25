#include "assembler.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
#include <iomanip>
using namespace std;

// Only the flags we want to demonstrate.
struct SimpleCPU {
    map<string, uint32_t> reg = {
        {"EAX", 0}, {"EBX", 0}, {"ECX", 0}, {"EDX", 0}
    };
    bool CF = false;  // Carry
    bool ZF = false;  // Zero
    bool SF = false;  // Sign
    bool OF = false;  // Signed overflow
    bool DF = false;  // Direction
};

// Read either a register value or an immediate number.
bool readValue(const SimpleCPU &cpu, const string &operand, uint32_t &value) {
    auto it = cpu.reg.find(operand);
    if (it != cpu.reg.end()) {
        value = it->second;
        return true;
    }
    if (!isNumber(operand)) return false;

    long long number = getNumber(operand);
    if (number < -2147483648LL || number > 4294967295LL)
        return false;
    value = static_cast<uint32_t>(number);
    return true;
}

// ADD/SUB/CMP update these four arithmetic flags.
void updateFlags(SimpleCPU &cpu, uint32_t a, uint32_t b,
                 uint32_t result, bool subtraction) {
    cpu.ZF = (result == 0);
    cpu.SF = (result & 0x80000000u) != 0;

    bool signA = (a & 0x80000000u) != 0;
    bool signB = (b & 0x80000000u) != 0;
    bool signR = (result & 0x80000000u) != 0;

    if (subtraction) {
        cpu.CF = (a < b); // Unsigned borrow
        cpu.OF = (signA != signB && signR != signA);
    } else {
        cpu.CF = (static_cast<uint64_t>(a) + b > 0xFFFFFFFFULL);
        cpu.OF = (signA == signB && signR != signA);
    }
}

void showFlags(const SimpleCPU &cpu, const string &instruction)
{
    cout << left << setw(23) << instruction
         << " | " << cpu.CF
         << "  | " << cpu.ZF
         << "  | " << cpu.SF
         << "  | " << cpu.OF
         << "  | " << cpu.DF
         << "  | 0x" << uppercase << hex << right
         << setw(8) << setfill('0') << cpu.reg.at("EAX")
         << dec << nouppercase << setfill(' ') << left << '\n';
}

// Called by main() only when the --eflags option is supplied.
void simulateEflagsForFile(const string &filename) {
    ifstream file(filename);
    if (!file) {
        cerr << "Cannot open EFLAGS input: " << filename << '\n';
        return;
    }

    SimpleCPU cpu;
    bool inText = false;
    string line;
    cout << "\n==================== EFLAGS SIMULATION ====================\n";

    cout << left << setw(23) << "Instruction"
        << " | CF | ZF | SF | OF | DF | EAX\n";

    cout << string(71, '-') << '\n';

    while (getline(file, line)) {
        stripComment(line);
        stringstream ss(line);
        string command;
        if (!(ss >> command)) continue;
        toUpperCase(command);

        if (command == "SECTION") {
            string section;
            ss >> section;
            toUpperCase(section);
            inText = (section == ".TEXT");
            continue;
        }
        if (!inText || command == "GLOBAL") continue;
        if (command.back() == ':') { // Ignore a label such as START:
            if (!(ss >> command)) continue;
            toUpperCase(command);
        }

        string arguments;
        getline(ss, arguments);
        vector<string> op;
        string error;
        if (!readOperands(arguments, op, error)) continue;
        if (findMatchingEncoding(command, op) == -1) continue;
        if (command == "RET") break;

        if (command == "STC") cpu.CF = true;
        else if (command == "CLC") cpu.CF = false;
        else if (command == "CMC") cpu.CF = !cpu.CF;
        else if (command == "STD") cpu.DF = true;
        else if (command == "CLD") cpu.DF = false;
        else if (command == "MOV" || command == "ADD" ||
                 command == "SUB" || command == "CMP") {
            if (op.size() != 2 || !cpu.reg.count(op[0])) continue;
            uint32_t b;
            if (!readValue(cpu, op[1], b)) continue;
            uint32_t a = cpu.reg[op[0]];

            if (command == "MOV") {
                cpu.reg[op[0]] = b; // MOV does not change flags.
            } else {
                bool subtraction = (command != "ADD");
                uint32_t result = subtraction ? a - b : a + b;
                updateFlags(cpu, a, b, result, subtraction);
                if (command != "CMP") cpu.reg[op[0]] = result;
            }
        } else {
            continue; // Other instructions are outside this small demo.
        }
        string instruction = command;
        if (!op.empty()) instruction += " " + op[0];
        if (op.size() == 2) instruction += ", " + op[1];
        showFlags(cpu, instruction);
    }
}
