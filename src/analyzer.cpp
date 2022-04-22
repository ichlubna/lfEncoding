#include <filesystem>
#include <set>
#include "analyzer.h"
#include "encoder.h"
#include "decoder.h"

void Analyzer::encode(std::string input, std::string output)
{ 
    auto dir = std::filesystem::directory_iterator(input); 
    std::set<std::filesystem::path> sorted;
    for(const auto &file : dir)
        sorted.insert(file);

    std::cout << "Encoding..." << std::endl;
    
    LoadingBar bar(sorted.size()+1, true);
    bar.print();

    size_t referenceID = sorted.size()/2;
    auto referenceFrame = *std::next(sorted.begin(), referenceID);
    Encoder encoder(referenceID, referenceFrame);

    bar.add();

    for(auto const &file : sorted)
        if(referenceFrame != file)
        {
            encoder << file;
            bar.add();
        }
    encoder.save(output);
    bar.add();
    std::cout << std::endl;
}

void Analyzer::decode(std::string input, float factor)
{
    Decoder decoder(input);
    decoder.decodeFrame(factor, Decoder::NEAREST);
}

Analyzer::Analyzer(std::string input, std::string output, float factor)
{
    if(factor<0)
        encode(input, output);
    else
        decode(input, factor);
}
