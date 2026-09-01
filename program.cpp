#include<iostream>
#include<fstream>
#include<vector>
#include<sstream>
#include<cctype>

using namespace std;

struct Opcode{
    string mnemonic;
    string opcode;
    int operand;
    string operand1;
    string operand2;
};
bool matchType(string actual, string expected){
            if(actual==expected){
                return true;
            }
            if(expected== "RM32" && actual =="REG32"){
                return true;
            }
            if(expected =="IMM16" &&actual == "IMM8"){
                 return true;
            }
            if(expected == "IMM32"&& actual =="IMM8"){
                return true;
            }
            if(expected =="IMM32" &&actual == "IMM16"){
                return true;
            }
            return false;
};

string getType(string operand){
    if (operand=="EAX" ||operand=="EBX"|| 
        operand=="ECX" ||operand=="EDX"||
        operand=="ESI" ||operand=="EDI"||
        operand=="EBP" ||operand=="ESP"){
            return "REG32";
        }
    if(operand.empty()){
        return "None";
    }
    if(operand=="CL"){
        return "CL";
    }
    if(operand[0]== '[' && operand[operand.length()-1]==']'){
        return "RM32";
    }
    bool number=true;
    for(int i=0;i<operand.length();i++){
        if(!isdigit(operand[i])){
            number=false;
            break;
        }
    }
    if(number){
        return "IMM32";

    }
    return "UNKNOWN";

}

int main(int argc, char *argv[]){


    if(argc !=3){
        cout <<"error";
        cout <<"Usage:"<<argv[0] <<"<opcode_file> <assembly_file>"<<endl;

        return 1;
    }
    ifstream opcodeFile(argv[1]);
    ifstream assemblyFile(argv[2]);

    if(!opcodeFile){
        cout<<"Error opening opcode file"<<endl;
        return 1;
    }
    if(!assemblyFile){
        cout<<"Error opening Assembly file"<<endl;
        return 1;
    }

    vector<Opcode> table;
    string line;                //for store line of opcodefile

    while(getline(opcodeFile, line)){
        stringstream ss(line);

       Opcode temp;

        ss>>temp.mnemonic;
        ss>>temp.opcode;
        ss>>temp.operand;
        ss>>temp.operand1;
        ss>>temp.operand2;

        table.push_back(temp);
    }

    string instruction;         //for storing assembly instruction
    while(getline(assemblyFile,instruction)){

        string mnemonic;
        string operand1;
        string operand2;

        stringstream ss(instruction);
        ss>>mnemonic;
        ss>>operand1;
        ss>>operand2;

        if(!operand1.empty() && operand1.back()==','){
            operand1.pop_back();

        }
        string type1=getType(operand1);
        string type2= getType(operand2);
        bool found=false;


        for(int i=0; i<table.size();i++){
            if(table[i].mnemonic != mnemonic){
                continue;
                
            }
            if(table[i].operand!=2){
                continue;

            }
            if(matchType(type1,table[i].operand1) &&matchType(type2, table[i].operand2)){
                found=true;
                break;
            }
        }
        if(found){
            cout << instruction << " : VALID" << endl;
   
        }
        else{
            cout << instruction << " : INVALID" << endl;
        }
        
    }
    return 0;


    
}
