#ifndef ANALYZER_H
#define ANALYZER_H

#include <iostream>
#include <exception>

class Analyzer
{
    public:    
    Analyzer(std::string input, std::string output);
    void encode(std::string input, std::string output); 
    void decode(std::string input);

    private:
    void printBar(int total, int processed) const;
};

#endif
