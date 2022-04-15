#include <filesystem>
#include <set>
#include "analyzer.h"
#include "encoder.h"
#include "decoder.h"

void Analyzer::encode(std::string input)
{
    auto dir = std::filesystem::directory_iterator(input);
    size_t count = std::distance(dir, std::filesystem::directory_iterator{});
    std::set<std::filesystem::path> sorted;
    for(const auto &file : dir)
        sorted.insert(file);

    size_t referenceFrame = sorted.size()/2;
    Encoder encoder(referenceFrame, *std::next(sorted.begin(), referenceFrame));
    for(auto const &file : sorted)
            encoder << file;
}

void Analyzer::decode(std::string input)
{

}

Analyzer::Analyzer(std::string input)
{
    if(std::filesystem::is_directory(input))
        encode(input);
    else
        decode(input);
}
