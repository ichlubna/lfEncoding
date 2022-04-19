extern "C" { 
#include <libavformat/avformat.h>
}
#include <fstream>
#include <vector>
#include "analyzer.h"

class Decoder
{
    public:
    enum Interpolation {NEAREST};
    Decoder(std::string path);
    ~Decoder();
    void decodeFrame(float factor, enum Interpolation);

    private:
    void initDecoder(std::string file);
    void openFile(std::string path);
    void loadPacketData(float factor, std::vector<uint8_t> *data);
    std::ifstream packetsFile;
    std::vector<uint32_t> offsets;
    AVFormatContext *formatContext;
    const AVCodec *codec;
    AVStream *stream;
    AVCodecContext *codecContext;
    AVPacket *decodingPacket;
};
