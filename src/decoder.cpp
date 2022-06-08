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

void Decoder::initDecoderParams(AVFormatContext **inputFormatContext, AVCodec **inputCodec, AVCodecContext **inputCodecContext, std::string inputFile)
{
    *inputFormatContext = avformat_alloc_context();
    if (avformat_open_input(inputFormatContext, inputFile.c_str(), nullptr, nullptr) != 0)
        throw std::runtime_error("Cannot open file: "+inputFile);
    if (avformat_find_stream_info(*inputFormatContext, nullptr) < 0) 
        throw std::runtime_error("Cannot find stream info in file: "+inputFile);
    auto videoStreamId = av_find_best_stream(*inputFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, const_cast<const AVCodec**>(inputCodec), 0);
    if(videoStreamId < 0)
        throw std::runtime_error("No video stream available");
    if(!codec)
        throw std::runtime_error("No suitable codec found");
    *inputCodecContext = avcodec_alloc_context3(*inputCodec);
    if(!codecContext)
        throw std::runtime_error("Cannot allocate codec context memory");
    if(avcodec_parameters_to_context(*inputCodecContext, (*inputFormatContext)->streams[videoStreamId]->codecpar)<0)
        throw std::runtime_error{"Cannot use the file parameters in context"};
     if(avcodec_open2(*inputCodecContext, *inputCodec, nullptr) < 0)
        throw std::runtime_error("Cannot open codec.");
}

void Decoder::initDecoder(std::string file)
{
    initDecoderParams(&formatContext, &codec, &codecContext, file);
    auto packet = av_packet_alloc();
    av_read_frame(formatContext, packet);
    //av_packet_copy_props(decodingPacket, packet);
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

void Decoder::decodeFrame(float factor, enum Interpolation interpolation, std::string outPath)
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
    {
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
            saveFrame(frame, outPath+"/"+std::to_string(indices.front())+".png");//std::to_string(decoded)+".png");
        decoded++; 
    }
    av_frame_free(&frame);

    //TODO blending?
}

void Decoder::decodeFrameClassic(float factor, [[maybe_unused]] enum Interpolation method, std::string file, std::string outPath)
{
    //TODO interpolation method

    AVFormatContext *classicFormatContext;
    AVCodec *classicCodec;
    AVCodecContext *classicCodecContext;

    initDecoderParams(&classicFormatContext, &classicCodec, &classicCodecContext, file);
    auto classicPacket = av_packet_alloc();
    auto classicFrame = av_frame_alloc();

    size_t position {static_cast<size_t>(round(factor*(offsets.size()-2)))};
    //assuming there are no keyframes except for the first one
    size_t counter = -1; 
    bool decoding = true;
    av_read_frame(classicFormatContext, classicPacket);
    while(decoding)
    {
        int err=0;
        while(err == 0)
        {
            if((err = avcodec_send_packet(classicCodecContext, classicPacket)) != 0)
                break;
            av_packet_unref(classicPacket);
            err = av_read_frame(classicFormatContext, classicPacket);
            if(err == AVERROR_EOF)
                classicPacket=nullptr;
        }

        bool waitForFrame = true;
        while(waitForFrame)
        {
            int err = avcodec_receive_frame(classicCodecContext, classicFrame);
            if(err == AVERROR(EAGAIN))
                break;
            if(err == AVERROR_EOF)
                waitForFrame = false;                
            else if(err < 0)
                throw std::runtime_error("Cannot receive frame");
            counter++;
            if(counter >= position)
            {
                decoding = false;
                break;
            }
        }
    }  
    saveFrame(classicFrame, outPath+"/"+std::to_string(counter)+"-classic.png");
    av_packet_free(&classicPacket);
    av_frame_free(&classicFrame);
}

void Decoder::decodeFrameClassicKey(float factor, [[maybe_unused]] enum Interpolation method, std::string file, std::string outPath)
{
    AVFormatContext *classicFormatContext;
    AVCodec *classicCodec;
    AVCodecContext *classicCodecContext;

    initDecoderParams(&classicFormatContext, &classicCodec, &classicCodecContext, file);
    AVStream *classicStream = classicFormatContext->streams[0];
    auto classicPacket = av_packet_alloc();
    auto classicFrame = av_frame_alloc();

    size_t position {static_cast<size_t>(round(factor*(offsets.size()-2)))};

    //NOT WORKING
    //size_t seekPosition = (position * classicStream->r_frame_rate.den * classicStream->time_base.den) /(classicStream->r_frame_rate.num * classicStream->time_base.num);
    //av_seek_frame(classicFormatContext, 0, seekPosition, AVSEEK_FLAG_ANY);
    //avformat_seek_file(classicFormatContext, -1, position, position, position, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
    for(int i=0; i<position; i++)
        av_read_frame(classicFormatContext, classicPacket);

    av_read_frame(classicFormatContext, classicPacket);
    avcodec_send_packet(classicCodecContext, classicPacket);
    avcodec_send_packet(classicCodecContext, nullptr);
    avcodec_receive_frame(classicCodecContext, classicFrame);
    saveFrame(classicFrame, outPath+"/"+std::to_string(position)+"-classicAllKey.png");
    av_packet_free(&classicPacket);
    av_frame_free(&classicFrame);
}

