#include <filesystem>
#include <set>
#include "analyzer.h"
#include "encoder.h"
#include "decoder.h"

void Analyzer::printBar(int total, int processed) const
{
    int progress = round((processed/total)*100);
    std::cout << "\r" << std::string(progress, '|') << " " << progress << "%" << std::flush;
}

void Analyzer::encode(std::string input, std::string output)
{ 
    auto dir = std::filesystem::directory_iterator(input); 
    std::set<std::filesystem::path> sorted;
    for(const auto &file : dir)
        sorted.insert(file);

    std::cout << "Encoding..." << std::endl;
    size_t total = sorted.size()+1;
    printBar(total, 0);
    int processed = 1;

    size_t referenceID = sorted.size()/2;
    auto referenceFrame = *std::next(sorted.begin(), referenceID);
    Encoder encoder(referenceID, referenceFrame);

    printBar(total, 1);

    for(auto const &file : sorted)
    {
        if(referenceFrame != file)
            encoder << file;
        processed++;
        printBar(total, processed);
    }
    encoder.save(output);
    processed++;
    printBar(total, processed);
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
