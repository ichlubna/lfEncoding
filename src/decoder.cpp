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
#include <cmath>
#include "decoder.h"
#include "utils.h"

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
            break;
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
    decodingPacket = av_packet_clone(packet);
    avcodec_send_packet(codecContext, packet);
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
    decodingPacket = av_packet_alloc();
    openFile(path);
    initDecoder(path+"/reference.ts");
}

Decoder::~Decoder()
{
    avformat_close_input(&formatContext);
    avformat_free_context(formatContext);
    avcodec_free_context(&codecContext);
    //av_packet_free(&decodingPacket);
}

void Decoder::saveFrame(AVFrame *frame, std::string path)
{
    auto outCodec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!outCodec) 
        throw std::runtime_error("Cannot find output codec");
    auto outContext = avcodec_alloc_context3(outCodec);
    if (!outContext)
        throw std::runtime_error("Cannot allocate output codec context");

    outContext->pix_fmt = AV_PIX_FMT_RGB24;
    outContext->height = frame->height;
    outContext->width = frame->width;
    outContext->time_base = AVRational{1,1};

    if (avcodec_open2(outContext, outCodec, nullptr) < 0) 
        throw std::runtime_error("Cannot allocate open codec");

    auto rgbFrame = Utils::ConvertedFrame(frame,  AV_PIX_FMT_RGB24);
    auto packet = av_packet_alloc();
    avcodec_send_frame(outContext, rgbFrame.getFrame());
    avcodec_send_frame(outContext, nullptr);
    bool waitForPacket = true;
    while(waitForPacket)
    {
        int err = avcodec_receive_packet(outContext, packet);
        if(err == AVERROR_EOF || err == AVERROR(EAGAIN))
            waitForPacket = false;                
        else if(err < 0)
            throw std::runtime_error("Cannot receive packet");
        break;
    }
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(packet->data), packet->size);
    avcodec_free_context(&outContext);
    av_packet_free(&packet);
}

void Decoder::decodeFrame(float factor, enum Interpolation interpolation)
{
    auto method = interpolation;
    float position {factor*(offsets.size()-2)};
    float fract = position-floor(position);
    if(fract < 0.00001)
        method = NEAREST;
    std::vector<size_t> indices;
    if(method == NEAREST)
        indices.push_back(round(position));
    else if(method == BLEND)
    {
        indices.push_back(static_cast<size_t>(floor(position)));
        indices.push_back(static_cast<size_t>(ceil(position)));
    }
    
    bool reference{false};
    for(auto const& index : indices)
    {std::cerr << index << std::endl;
        size_t id = index;
        if(index > offsets.back())
            id = index-1;
        else if(index == offsets.back())
        {
            reference = true;
            continue;
        }
         
        std::vector<uint8_t> data;
        loadPacketData(id, &data);
        //inserting NAL start code - AnnexB, and header
        data.insert(data.begin(), {0,0,0,1,2,1,208,9,126});
        av_packet_from_data(decodingPacket, data.data(), data.size());
        decodingPacket->flags = 0; 
        //decodingPacket->pts = index;
        //decodingPacket->dts = index;
        if(avcodec_send_packet(codecContext, decodingPacket) < 0)
            throw std::runtime_error("Cannot send packet for decoding");
    }
    avcodec_send_packet(codecContext, nullptr);
    
    auto frame = av_frame_alloc();
    int decoded = 0;
    while(true)
    {
        int err = avcodec_receive_frame(codecContext, frame);
        if(err == AVERROR_EOF || err == AVERROR(EAGAIN))
            break;
        else if(err < 0)
            throw std::runtime_error("Cannot receive frame");
        if(decoded > 0 || reference)
            saveFrame(frame, "./output"+std::to_string(decoded)+".png");
        decoded++; 
    }
    av_frame_free(&frame);

    //TODO blending?
}
