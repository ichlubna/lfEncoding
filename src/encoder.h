#include <vector>
#include "analyzer.h"

class Encoder
{
    public:
    Encoder(uint32_t refID, std::string refFile);
    friend void operator<<(Encoder &e, std::string file){e.encodeFrame(file);}

    private:
    std::vector<uint8_t> data;
    std::vector<uint32_t> offsets;
    uint32_t referenceIndex;
    std::string referenceFile;
    void encodeFrame(std::string file);
    void save(std::string path);

    class PairEncoder
    {
        public:
        PairEncoder(std::string ref, std::string frame) : referenceFile(ref), frameFile(frame) {encode();};
        const std::vector<uint8_t>* getFramePacket() const {return &framePacket;};
        const std::vector<uint8_t>* getPacket() const {return &referencePacket;};

        private:
        void encode();
        std::string referenceFile; 
        std::string frameFile; 
        std::vector<uint8_t> framePacket;
        std::vector<uint8_t> referencePacket;
    };
};
