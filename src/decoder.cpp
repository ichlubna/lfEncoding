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

void Decoder::initDecoder(std::string file)
{
    formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, file.c_str(), nullptr, nullptr) != 0)
        throw std::runtime_error("Cannot open file: "+file);
    if (avformat_find_stream_info(formatContext, nullptr) < 0) 
        throw std::runtime_error("Cannot find stream info in file: "+file);
    AVCodec *codec;
    auto videoStreamId = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, const_cast<const AVCodec**>(&codec), 0);
    if(videoStreamId < 0)
        throw std::runtime_error("No video stream available");
    if(!codec)
        throw std::runtime_error("No suitable codec found");
    codecContext = avcodec_alloc_context3(codec);
    if(!codecContext)
        throw std::runtime_error("Cannot allocate codec context memory");
    if(avcodec_parameters_to_context(codecContext, formatContext->streams[videoStreamId]->codecpar)<0)
        throw std::runtime_error{"Cannot use the file parameters in context"};
     if(avcodec_open2(codecContext, codec, nullptr) < 0)
        throw std::runtime_error("Cannot open codec.");
    auto packet = av_packet_alloc();
    av_read_frame(formatContext, packet);
    av_packet_copy_props(decodingPacket, packet);
    decodingPacket->flags = 0;
    //decodingPacket = av_packet_clone(packet);
    avcodec_send_packet(codecContext, packet);
}


void Decoder::loadPacketData(float factor, std::vector<uint8_t> *data)
{
    size_t index = round(factor*offsets.size()-2);
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
    decodingPacket = av_packet_alloc();
    openFile(path);
    initDecoder(path+"/reference.mkv");
}

Decoder::~Decoder()
{
    avformat_close_input(&formatContext);
    avformat_free_context(formatContext);
    avcodec_free_context(&codecContext);
    av_packet_free(&decodingPacket);
}

void Decoder::decodeFrame(float factor, enum Interpolation interpolation)
{
    std::vector<uint8_t> data;
    loadPacketData(factor, &data);
    av_packet_from_data(decodingPacket, data.data(), data.size());
    decodingPacket->buf->size=data.size();
    //decodingPacket->data = data.data();
    //decodingPacket->size = data.size();
    
    auto frame = av_frame_alloc();
    decodingPacket->flags = 0;
    if(avcodec_send_packet(codecContext, decodingPacket) < 0)
        throw std::runtime_error("Cannot send packet for decoding");
    avcodec_send_packet(codecContext, nullptr);
    int decoded = 0;
    while(decoded < 2)
    {
        int err = avcodec_receive_frame(codecContext, frame);
        if(err == AVERROR_EOF || err == AVERROR(EAGAIN))
            decoded = 999;
        else if(err < 0)
            throw std::runtime_error("Cannot receive frame");
        if(err >= 0)
           decoded++; 
        std::cerr << frame->width;
    }
    av_frame_free(&frame);
}
