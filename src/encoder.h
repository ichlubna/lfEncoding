extern "C" { 
#include <libavformat/avformat.h>
}
#include <filesystem>
#include <vector>
#include <set>
#include "analyzer.h"

class Encoder
{
    public:
    Encoder(uint32_t refID, std::string refFile);
    void save(std::string path);
    static const AVPixelFormat outputPixelFormat{AV_PIX_FMT_YUV444P};
    friend void operator<<(Encoder &e, std::string file){e.encodeFrame(file);}
    void encodeClassic(std::set<std::filesystem::path> *sortedFiles, std::string path, bool allKey=false) const;

    private:
    std::vector<uint8_t> data;
    std::vector<uint32_t> offsets;
    uint32_t referenceIndex;
    std::string referenceFile;
    void encodeFrame(std::string file);
    void addData(const std::vector<uint8_t> *packetData);
    void encodeReference(std::string path);

    class FFMuxer
    {
        public:
        FFMuxer(std::string fileName, const AVCodecContext *ecnoderCodecContext);
        ~FFMuxer();
        friend void operator<<(FFMuxer &m, AVPacket *packet){m.writePacket(packet);}
        void finish();

        private:
        void writePacket(AVPacket *packet);
        AVFormatContext *muxerFormatContext;
        bool finished{false};
    };

    class FFEncoder
    {
        public:
        FFEncoder(size_t width, size_t height, AVPixelFormat pixFmt, bool allKey=false);
        ~FFEncoder();
        friend void operator<<(FFEncoder &e, AVFrame *frame){e.encodeFrame(frame);}
        friend void operator>>(FFEncoder &e, AVPacket **packetPtr){*packetPtr = e.retrievePacket();}
        const AVCodecContext *getCodecContext() const {return codecContext;};

        private:
        void encodeFrame(AVFrame *frame);
        AVPacket* retrievePacket();
        //AVFormatContext *formatContext;
        const AVCodec *codec;
        AVStream *stream;
        AVCodecContext *codecContext;
        AVPacket *packet;
    };

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
        std::string referenceFile; 
        std::string frameFile; 
        std::vector<uint8_t> framePacket;
        std::vector<uint8_t> referencePacket;
    };
};
