#ifndef ANALYZER_H
#define ANALYZER_H

#include <iostream>
#include <exception>

class Analyzer
{
    public:    
    Analyzer(std::string input);
    void encode(std::string input); 
    void decode(std::string input);

    private:
};

#endif
