#include <chrono>
#include "analyzer.h"
#include "encoder.h"
#include "decoder.h"
#include "loadingBar/loadingbar.hpp"

void Analyzer::encode(std::string input, std::string output, size_t crf)
{ 
    auto dir = std::filesystem::directory_iterator(input); 
    std::set<std::filesystem::path> sorted;
    for(const auto &file : dir)
        sorted.insert(file);

    std::cout << "Encoding..." << std::endl;
    
    LoadingBar bar(sorted.size()+3, true);
    bar.print();

    size_t referenceID = sorted.size()/2;
    auto referenceFrame = *std::next(sorted.begin(), referenceID);

    auto encodeStart = std::chrono::steady_clock::now();

    Encoder encoder(referenceID, referenceFrame, crf);

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
    encoder.encodeClassic(&sorted, output+"/classic.ts");
    auto classicEnd = std::chrono::steady_clock::now();
    bar.add();
    
    auto classicKeyStart = std::chrono::steady_clock::now();
    encoder.encodeClassic(&sorted, output+"/classicKey.ts", true);
    auto classicKeyEnd = std::chrono::steady_clock::now();
    bar.add();

    std::cout << std::endl;
    std::cout << "Encoding time: " << std::chrono::duration_cast<std::chrono::milliseconds>(encodeEnd-encodeStart).count()/1000.0f << "s" << std::endl;
    std::cout << "Encoding classic time: " << std::chrono::duration_cast<std::chrono::milliseconds>(classicEnd-classicStart).count()/1000.0f << "s" << std::endl;
    std::cout << "Encoding classic all-key time: " << std::chrono::duration_cast<std::chrono::milliseconds>(classicKeyEnd-classicKeyStart).count()/1000.0f << "s" << std::endl;
    size_t referenceSize =  std::filesystem::file_size(output+"/reference.ts"); 
    size_t offsetsSize = std::filesystem::file_size(output+"/offsets.lfo"); 
    size_t packetsSize = std::filesystem::file_size(output+"/packets.lfp"); 
    size_t classicSize = std::filesystem::file_size(output+"/classic.ts"); 
    size_t classicKeySize = std::filesystem::file_size(output+"/classicKey.ts"); 
    size_t totalSize = referenceSize+offsetsSize+packetsSize; 
    std::cout << "Encoded size: total - " << totalSize << std::endl;
    std::cout << "  reference - " << (static_cast<float>(referenceSize)/totalSize)*100 << "%" << std::endl;
    std::cout << "  offsets - " << (static_cast<float>(offsetsSize)/totalSize)*100 << "%" << std::endl;
    std::cout << "  packets - " << (static_cast<float>(packetsSize)/totalSize)*100 << "%" << std::endl;
    std::cout << "Encoded classic size: total - " << classicSize << std::endl;
    std::cout << "Encoded classic all-key size: total - " << classicKeySize << std::endl;
}

void Analyzer::decode(std::string input, float factor, int method, std::string outPath)
{
    Decoder::Interpolation methodName;
    switch(method)
    {
        case 0:
        methodName = Decoder::Interpolation::NEAREST;
        break;
        
        case 1:
        methodName = Decoder::Interpolation::BLEND;
        break;
    }

    auto start = std::chrono::steady_clock::now();
    Decoder decoder(input);
    decoder.decodeFrame(factor, methodName, outPath);
    auto end = std::chrono::steady_clock::now();
    std::cout << "Decoding time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << "ms" << std::endl;

    start = std::chrono::steady_clock::now();
    decoder.decodeFrameClassic(factor, methodName, input+"/classic.ts", outPath);
    end = std::chrono::steady_clock::now();
    std::cout << "Decoding time classic: " << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << "ms" << std::endl;
    
    start = std::chrono::steady_clock::now();
    decoder.decodeFrameClassicKey(factor, methodName, input+"/classicKey.ts", outPath);
    end = std::chrono::steady_clock::now();
    std::cout << "Decoding time classic all-key: " << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << "ms" << std::endl;
}

Analyzer::Analyzer(std::string input, std::string output, size_t crf, float factor, int method)
{
    if(factor<0)
        encode(input, output, crf);
    else
        decode(input, factor, method, output);
}

