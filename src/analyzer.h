#ifndef ANALYZER_H
#define ANALYZER_H

#include <iostream>
#include <exception>
#include <math.h>
#include <iomanip>

class Analyzer
{
    public:    
    Analyzer(std::string input, std::string output, size_t crf, float factor=-1, bool cpuDecoding=false);
    void encode(std::string input, std::string output, size_t crf); 
    void decode(std::string input, float factor, int method, std::string outPath, bool cpuDecoding);

    private:
};

#endif
