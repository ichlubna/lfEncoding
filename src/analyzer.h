#ifndef ANALYZER_H
#define ANALYZER_H

#include <iostream>
#include <exception>
#include <math.h>
#include <iomanip>

class Analyzer
{
    public:    
    Analyzer(std::string input, std::string output, float factor=-1, int method=0);
    void encode(std::string input, std::string output); 
    void decode(std::string input, float factor, int method, std::string outPath);

    private:
};

#endif
