#include <chrono>
#include "analyzer.h"
#include "encoder.h"
#include "decoder.h"
#include "loadingBar/loadingbar.hpp"

void Analyzer::encode(std::string input, std::string output)
{ 
    auto dir = std::filesystem::directory_iterator(input); 
    std::set<std::filesystem::path> sorted;
    for(const auto &file : dir)
        sorted.insert(file);

    std::cout << "Encoding..." << std::endl;
    
    LoadingBar bar(sorted.size()+2, true);
    bar.print();

    size_t referenceID = sorted.size()/2;
    auto referenceFrame = *std::next(sorted.begin(), referenceID);

    auto encodeStart = std::chrono::steady_clock::now();

    Encoder encoder(referenceID, referenceFrame);

    bar.add();

    for(auto const &file : sorted)
        if(referenceFrame != file)
        {
            encoder << file;
            bar.add();
        }
    encoder.save(output);
    auto encodeEnd = std::chrono::steady_clock::now();
    bar.add();
    
    auto classicStart = std::chrono::steady_clock::now();
    encoder.encodeClassic(&sorted, output);
    auto classicEnd = std::chrono::steady_clock::now();
    bar.add();

    std::cout << "Encoding time: " << std::chrono::duration_cast<std::chrono::milliseconds>(encodeEnd-encodeStart).count() << "ms" << std::endl;
    std::cout << "Encoding reference time: " << std::chrono::duration_cast<std::chrono::milliseconds>(classicEnd-classicStart).count() << "ms" << std::endl;
}

void Analyzer::decode(std::string input, float factor, int method)
{
    auto start = std::chrono::steady_clock::now();
    Decoder decoder(input);
    switch(method)
    {
        case 0:
        decoder.decodeFrame(factor, Decoder::Interpolation::NEAREST);
        break;
        
        case 1:
        decoder.decodeFrame(factor, Decoder::Interpolation::BLEND);
        break;
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "Decoding time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << "ms" << std::endl;
}

Analyzer::Analyzer(std::string input, std::string output, float factor, int method)
{
    if(factor<0)
        encode(input, output);
    else
        decode(input, factor, method);
}

