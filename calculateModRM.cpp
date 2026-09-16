#include <iostream>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <string>
#include "lib.h"

using namespace std;

string toUpper(string str)
{
	for (char &c : str)
		c = toupper(c);

	return str;
}

int getRegisterCode(string reg)
{
	reg = toUpper(reg);

	if (reg == "EAX")
		return 0;
	if (reg == "ECX")
		return 1;
	if (reg == "EDX")
		return 2;
	if (reg == "EBX")
		return 3;
	if (reg == "ESP")
		return 4;
	if (reg == "EBP")
		return 5;
	if (reg == "ESI")
		return 6;
	if (reg == "EDI")
		return 7;

	return -1;
}

bool isReg(string &operand)
{
	if (operand == "EAX" || operand == "EBX" || operand == "ECX" || operand == "EDX" || operand == "ESI" || operand == "EDI" || operand == "EBP" || operand == "ESP")
		return true;
	return false;
}

string getOperandType(string operand)
{
	operand = toUpper(operand);

	// 32-bit registers
	if (isReg(operand))
		return "REG32";

	// CL
	if (operand == "CL")
		return "CL";

	// Memory operand
	if (operand.length() >= 2 && operand[0] == '[' && operand[operand.length() - 1] == ']')
		return "RM32";

	// Immediate
	bool number = true;
	int start = 0;

	if (operand.length() == 0)
		return "UNKNOWN";

	if (operand[0] == '-' || operand[0] == '+')
		start = 1;

	if (start == operand.length())
		return "UNKNOWN";

	for (int i = start; i < operand.length(); i++)
	{
		if (!isdigit(operand[i]))
		{
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

int getMod(string operand)
{
	string type = getOperandType(operand);

	// Register → MOD = 11
	if (type == "REG32")
		return 3;

	// Memory operand
	if (type == "RM32")
	{
		// Remove [ and ]
		string address =
			operand.substr(1, operand.length() - 2);

		// ---------------------------------------
		// Check for displacement
		// ---------------------------------------

		size_t pos = address.find('+');

		if (pos == string::npos)
			pos = address.find('-');

		// No + or -
		if (pos == string::npos)
			return 0;

		// ---------------------------------------
		// Get the part after + or -
		// ---------------------------------------

		string displacement =
			address.substr(pos + 1);

		// ---------------------------------------
		// If this contains '*',
		// it is a scaled index, not displacement
		// ---------------------------------------

		if (displacement.find('*') != string::npos)
		{
			// Example:
			// [EBX+ESI*4]
			// No displacement
			return 0;
		}

		// ---------------------------------------
		// Actual displacement
		// ---------------------------------------

		int value;

		if (address[pos] == '-')
			value = -stoi(displacement);
		else
			value = stoi(displacement);

		// 8-bit displacement
		if (value >= -128 && value <= 127)
			return 1;

		// 32-bit displacement
		return 2;
	}

	return -1;
}
int getRM(string operand)
{
	string type = getOperandType(operand);

	// Register operand
	if (type == "REG32")
	{
		return getRegisterCode(operand);
	}

	// Memory operand
	if (type == "RM32")
	{
		string address = operand.substr(1, operand.length() - 2);

		// Remove displacement
		size_t pos = address.find('+');

		if (pos == string::npos)
			pos = address.find('-');

		if (pos != string::npos)
		{
			address = address.substr(0, pos);
		}
		return getRegisterCode(address);
	}
	return -1;
}

unsigned char calculateModRM(const Encoding &enc, string operand1, string operand2)
{
	int mod;
	int reg;
	int rm;

	string type1 = getOperandType(operand1);
	string type2 = getOperandType(operand2);

	// cout << "Operand1: " << operand1 << " Type: " << type1 << endl;
	// cout << "Operand2: " << operand2 << " Type: " << type2 << endl;

	// /r  → encodingRule = 1
	if (enc.encodingRule == 1)
	{
		// REG32, RM32
		// REG32, REG32 is also valid because MOD = 11
		if (type1 == "REG32" && (type2 == "RM32" || type2 == "REG32"))
		{
			reg = getRegisterCode(operand1);
			mod = getMod(operand2);
			rm = getRM(operand2);
		}
		// RM32, REG32
		else if ((type1 == "RM32" || type1 == "REG32") && type2 == "REG32")
		{
			reg = getRegisterCode(operand2);
			mod = getMod(operand1);
			rm = getRM(operand1);
		}
		else
		{
			cout << "Invalid operands for /r\n";
			return 0;
		}
	}

	// /0 ... /7
	else if (enc.encodingRule >= 2 && enc.encodingRule <= 9)
	{
		reg = enc.encodingRule - 2;
		mod = getMod(operand1);
		rm = getRM(operand1);
	}
	else
	{
		// No ModR/M
		return 0;
	}

	if (mod == -1 || reg == -1 || rm == -1)
	{
		cout << "Invalid ModR/M fields\n";
		return 0;
	}
	return (mod << 6) | (reg << 3) | rm;
}

bool needsSIB(string operand)
{
	// Not a memory operand
	if (getOperandType(operand) != "RM32")
		return false;

	// Remove [ and ]
	string address =
		operand.substr(1, operand.length() - 2);

	// ---------------------------------------
	// ESP as base always requires SIB
	// ---------------------------------------

	if (address.find("ESP") != string::npos)
	{
		// ESP can be a base register.
		// In 32-bit ModR/M encoding,
		// RM = 100 means SIB follows.
		return true;
	}

	// ---------------------------------------
	// Scaled index requires SIB
	// ---------------------------------------

	if (address.find('*') != string::npos)
		return true;

	// ---------------------------------------
	// Normal [register + displacement]
	// does not require SIB
	// ---------------------------------------

	return false;
}

unsigned char calculateSIB(string operand){
	// Remove [ and ]
	string address =
		operand.substr(1, operand.length() - 2);

	int scale = 0; // 00 -> ×1
	int index = 4; // 100 -> no index
	int base = 5;  // temporary

	// ---------------------------------------
	// Find '*'
	// ---------------------------------------

	size_t star = address.find('*');

	// ---------------------------------------
	// No scaled index
	// Example:
	// [ESP]
	// [ESP+8]
	// ---------------------------------------

	if (star == string::npos)
	{
		// ESP is special.
		// It requires SIB.
		base = getRegisterCode("ESP");

		// No index
		index = 4;

		// Scale = ×1
		scale = 0;
	}
	else
	{
		// ---------------------------------------
		// Example:
		// EBX+ESI*4
		// ---------------------------------------

		// Find index register before '*'
		size_t indexStart = star;

		while (indexStart > 0 &&
			   address[indexStart - 1] != '+' &&
			   address[indexStart - 1] != '-')
		{
			indexStart--;
		}

		string indexReg =
			address.substr(
				indexStart,
				star - indexStart);

		index = getRegisterCode(indexReg);

		// ---------------------------------------
		// Find scale
		// ---------------------------------------

		string scaleValue =
			address.substr(star + 1);

		// Remove anything after + or -
		size_t pos = scaleValue.find('+');

		if (pos == string::npos)
			pos = scaleValue.find('-');

		if (pos != string::npos)
			scaleValue = scaleValue.substr(0, pos);

		int value = stoi(scaleValue);

		if (value == 1)
			scale = 0;
		else if (value == 2)
			scale = 1;
		else if (value == 4)
			scale = 2;
		else if (value == 8)
			scale = 3;

		// ---------------------------------------
		// Find base register
		// ---------------------------------------

		string baseReg =
			address.substr(0, indexStart);

		if (!baseReg.empty() &&
			baseReg.back() == '+')
		{
			baseReg.pop_back();
		}

		base = getRegisterCode(baseReg);
	}

	// ---------------------------------------
	// Construct SIB byte
	// ---------------------------------------

	return (scale << 6) |
		   (index << 3) |
		   base;
}

void modRM(unordered_map<char, unordered_map<string, vector<Encoding>>> &ds, FILE *fp)
{
	char line[256];

	cout << "------------------- Second Pass -------------------" << endl;
	fseek(fp, 0, SEEK_SET);

	while (fgets(line, sizeof(line), fp))
	{
		int i = 0;

		// Skip leading spaces
		while (line[i] == ' ' || line[i] == '\t')
			i++;

		// Empty line
		if (line[i] == '\0' || line[i] == '\n')
			continue;

		// Read mnemonic
		string mnemonic;
		while (isalpha(line[i]))
		{
			mnemonic += line[i];
			i++;
		}
		mnemonic = toUpper(mnemonic);
		if (mnemonic.empty())
			continue;

		// Read operands
		// Same parsing used in validation
		vector<string> operands;
		while (line[i] == ' ' || line[i] == '\t')
			i++;

		if (line[i] != '\0' && line[i] != '\n')
		{
			while (true)
			{
				// Skip spaces
				while (line[i] == ' ' || line[i] == '\t')
					i++;

				if (line[i] == '\0' || line[i] == '\n')
					break;

				// Read one operand
				string operand;
				while (line[i] != ',' && line[i] != ' ' && line[i] != '\t' && line[i] != '\n' && line[i] != '\0')
				{
					operand += line[i];
					i++;
				}
				operand = toUpper(operand);
				if (!operand.empty())
					operands.push_back(operand);

				// Skip spaces
				while (line[i] == ' ' || line[i] == '\t')
					i++;

				// End of line
				if (line[i] == '\0' || line[i] == '\n')
					break;

				// Comma found
				if (line[i] == ',')
				{
					i++;
					// Skip spaces after comma
					while (line[i] == ' ' || line[i] == '\t')
						i++;
					continue;
				}
				break;
			}
		}

		// Get operands
		string operand1 = "NONE";
		string operand2 = "NONE";

		if (operands.size() >= 1)
			operand1 = operands[0];

		if (operands.size() >= 2)
			operand2 = operands[1];

		// Find instruction in data structure
		char firstChar = mnemonic[0];
		auto outer = ds.find(firstChar);
		if (outer == ds.end())
			continue;

		auto inner = outer->second.find(mnemonic);
		if (inner == outer->second.end())
			continue;

		// Find corresponding encoding
		for (const Encoding &enc : inner->second)
		{
			bool match = false;
			string type1 = getOperandType(operand1);
			string type2 = getOperandType(operand2);

			// 0 operand instruction
			if (enc.operandCount == 0 && operands.size() == 0)
			{
				match = true;
			}

			// 1 operand instruction
			else if (enc.operandCount == 1 && operands.size() == 1)
			{
				if (type1 == enc.operand1)
					match = true;
			}

			// 2 operand instruction
			else if (enc.operandCount == 2 && operands.size() == 2)
			{
				if (enc.encodingRule == 1)
				{
					if (type1 == "REG32" && (type2 == "RM32" || type2 == "REG32"))
						match = true;
					else if ((type1 == "RM32" || type1 == "REG32") && type2 == "REG32")
						match = true;
				}

				// Other two-operand encodings
				else
				{
					if (type1 == enc.operand1 && type2 == enc.operand2)
						match = true;
				}
			}

			// Encoding found
			if (!match)
				continue;

			cout << "\nInstruction: " << mnemonic << endl;
			cout << "Opcode: " << enc.opcode << endl;

			// Calculate ModR/M only if required
			if (enc.modRM == 1)
			{
				unsigned char modrm = calculateModRM(enc, operand1, operand2);
				cout << "ModR/M: " << (int)modrm << endl;
			}

			string sibOperand = "NONE";

			if (getOperandType(operand1) == "RM32")
				sibOperand = operand1;
			else if (getOperandType(operand2) == "RM32")
				sibOperand = operand2;

				cout << "Sib : " << enc.sib << endl;
				cout << "Need sib : " << needsSIB(sibOperand) << endl;
			if (enc.sib == 1 && needsSIB(sibOperand))
			{
				unsigned char sib = calculateSIB(sibOperand);
				cout << "SIB: " << (int)sib << endl;
			}

			// One encoding found
			break;
		}
	}
}