extern "C" { 
#include <libavformat/avformat.h>
}
#include <vector>
#include "analyzer.h"

class Encoder
{
    public:
    Encoder(uint32_t refID, std::string refFile);
    void save(std::string path);
    friend void operator<<(Encoder &e, std::string file){e.encodeFrame(file);}

    private:
    std::vector<uint8_t> data;
    std::vector<uint32_t> offsets;
    uint32_t referenceIndex;
    std::string referenceFile;
    void encodeFrame(std::string file);
    void addData(const std::vector<uint8_t> *packetData);

    class PairEncoder
    {
        public:
        class Frame
        {
            public:
            Frame(std::string file);
            ~Frame();
            const AVFrame* getFrame() const { return frame; };

            private:
            AVFormatContext *formatContext;
            AVCodec *codec;
            AVStream *stream;
            AVCodecContext *codecContext;
            AVFrame *frame;            
            AVPacket *packet;
        };

        PairEncoder(std::string ref, std::string frame) : referenceFile(ref), frameFile(frame) {encode();};
        const std::vector<uint8_t>* getFramePacket() const {return &framePacket;};
        const std::vector<uint8_t>* getReferencePacket() const {return &referencePacket;};

        private:
        void encode();
        AVPixelFormat outputPixelFormat{AV_PIX_FMT_YUV444P};
        std::string referenceFile; 
        std::string frameFile; 
        std::vector<uint8_t> framePacket;
        std::vector<uint8_t> referencePacket;
    };
};
