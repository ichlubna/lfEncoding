extern "C" { 
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#include <stdexcept>
#include <zlib.h>
#include <filesystem>
#include <fstream>
#include "decoder.h"

void Decoder::openFile(std::string path)
{
    auto offsetsFile = gzopen((path+"/offsets.lfo").c_str(), "rb");
    constexpr size_t LOAD_SIZE{4096};
    uint8_t buffer[LOAD_SIZE];
    uint32_t *intBuffer = reinterpret_cast<uint32_t*>(buffer);
    int size{0};
    while(true)
    {
        size = gzread(offsetsFile, buffer, LOAD_SIZE);
        if(size == 0)
            return;
        offsets.insert(offsets.end(), intBuffer, intBuffer+size/4);
    }

   /* 
    size_t offsetsBufferSize{1000};
    offsets.resize(offsetsBufferSize);
    while(true)
        if(gzread(offsetsFile, offsets.data(), offsetsBufferSize*8) == 0)
            return;*/

    packetsFile.open(path+"/packets.lfp", std::ifstream::in | std::ifstream::binary);    
}

void Decoder::initDecoder()
{
    formatContext = avformat_alloc_context();
    codec = avcodec_find_encoder(AV_CODEC_ID_HEVC);
    stream = avformat_new_stream(formatContext, codec); 
    codecContext = avcodec_alloc_context3(codec);
    if(avcodec_open2(codecContext, codec, nullptr) < 0)
        throw std::runtime_error("Cannot open codec.");
}


void Decoder::loadPacketData(size_t index, std::vector<uint8_t> *data)
{
    if (packetsFile.good())
    {
        packetsFile.seekg(offsets[index], std::ios::beg);
        size_t size = offsets[index+1] - offsets[index]; 
        data->resize(size);
        packetsFile.read(reinterpret_cast<char*>(data->data()), size); 
    }
}

Decoder::Decoder(std::string path)
{
    openFile(path);
    initDecoder();
}

Decoder::~Decoder()
{
    avformat_close_input(&formatContext);
    avformat_free_context(formatContext);
    avcodec_free_context(&codecContext);
}

void Decoder::decodeFrame(float factor, enum Interpolation interpolation)
{
    auto packet = av_packet_alloc();
    std::vector<uint8_t> data;
    loadPacketData(0, &data);
    packet->data = data.data();    
    //metadata and check codec
 
    auto frame = av_frame_alloc();
    avcodec_send_packet(codecContext, packet);
    avcodec_send_packet(codecContext, nullptr);
    bool waitForFrame = true;
    while(waitForFrame)
    {
        int err = avcodec_receive_frame(codecContext, frame);
        if(err == AVERROR_EOF || err == AVERROR(EAGAIN))
            waitForFrame = false;                
        else if(err < 0)
            throw std::runtime_error("Cannot receive frame");
        if(err >= 0)
            waitForFrame = false;
    }
    av_frame_free(&frame);
    av_packet_free(&packet);
}
