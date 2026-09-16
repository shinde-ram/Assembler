#include <iostream>

#include <unordered_map>
#include <vector>
#include <sstream>
#include <string>
#include "lib.h"

using namespace std;

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

    //Calculate modRM
    modRM(ds, fp);
    
    fclose(fp);

    return 0;
}