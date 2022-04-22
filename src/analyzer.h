#ifndef ANALYZER_H
#define ANALYZER_H

#include <iostream>
#include <exception>
#include <math.h>
#include <iomanip>

class LoadingBar
{
    public:
    LoadingBar(size_t total, bool printWidthAdd=false) : totalCount{total}, addPrint{printWidthAdd} {};
    size_t add(size_t number=1){count+=number; if(addPrint) print(); return count;};
    void print()
    {
        float progress = (static_cast<float>(count)/totalCount)*100;
        std::cout << "\r" << std::setprecision(2) << std::fixed << std::string(static_cast<size_t>(round(progress)), '|') << " " << progress << "%" << std::flush;
    }

    private:
    size_t totalCount;
    size_t count{0};
    bool addPrint{false};
};

class Analyzer
{
    public:    
    Analyzer(std::string input, std::string output, float factor=-1);
    void encode(std::string input, std::string output); 
    void decode(std::string input, float factor);

    private:
};

#endif
