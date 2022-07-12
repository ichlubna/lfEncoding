extern "C" { 
#include <libavformat/avformat.h>
}
#include <fstream>
#include <vector>
#include "analyzer.h"

class Decoder
{
    public:
    enum Interpolation {NEAREST, BLEND};
    Decoder(std::string path, bool cpuDecoding);
    ~Decoder();
    void decodeFrame(float factor, enum Interpolation, std::string outPath);    
    void decodeFrameClassic(float factor, enum Interpolation method, std::string file, std::string outPath);
    void decodeFrameClassicKey(float factor, enum Interpolation method, std::string file, std::string outPath);

    private:
    bool GPU{true};
    void initDecoderParams(AVFormatContext **inputFormatContext, AVCodec **inputCodec, AVCodecContext **inputCodecContext, std::string inputFile);
    void initDecoder(std::string file);
    void openFile(std::string path);
    void loadPacketData(size_t index, std::vector<uint8_t> *data);
    void saveFrame(AVFrame *frame, std::string path);
    std::ifstream packetsFile;
    std::vector<uint32_t> offsets;
    AVFormatContext *formatContext;
    AVCodec *codec;
    AVCodecContext *codecContext;
    AVPacket *decodingPacket;
};
