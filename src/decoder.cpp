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
#include <chrono>
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
    packetsFile.open(path+"/packets.lfp", std::ifstream::in | std::ifstream::binary); 
}

void Decoder::initDecoderParams(AVFormatContext **inputFormatContext, AVCodec **inputCodec, AVCodecContext **inputCodecContext, std::string inputFile)
{
    //av_log_set_level(AV_LOG_TRACE);
    *inputFormatContext = avformat_alloc_context();
    if (avformat_open_input(inputFormatContext, inputFile.c_str(), nullptr, nullptr) != 0)
        throw std::runtime_error("Cannot open file: "+inputFile);
    
    (*inputFormatContext)->probesize = 42000000;

    if (avformat_find_stream_info(*inputFormatContext, nullptr) < 0) 
        throw std::runtime_error("Cannot find stream info in file: "+inputFile);
    auto videoStreamId = av_find_best_stream(*inputFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, const_cast<const AVCodec**>(inputCodec), 0);
    if(videoStreamId < 0)
        throw std::runtime_error("No video stream available");
    if(!inputCodec)
        throw std::runtime_error("No suitable codec found");
    *inputCodecContext = avcodec_alloc_context3(*inputCodec);
    if(!inputCodecContext)
        throw std::runtime_error("Cannot allocate codec context memory");
    if(avcodec_parameters_to_context(*inputCodecContext, (*inputFormatContext)->streams[videoStreamId]->codecpar)<0)
        throw std::runtime_error{"Cannot use the file parameters in context"};

    if(GPU)
    {
        AVBufferRef *deviceContext;
        const AVCodecHWConfig *config;
        for(int i=0;; i++)
            if(!(config = avcodec_get_hw_config(*inputCodec, i)))
                throw std::runtime_error("No HW config for codec");
            else if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == AV_HWDEVICE_TYPE_CUDA)//AV_HWDEVICE_TYPE_VDPAU)
                break;       

            if(av_hwdevice_ctx_create(&deviceContext, config->device_type, NULL, NULL, 0) < 0)
            throw std::runtime_error("Cannot create HW device");
        (*inputCodecContext)->hw_device_ctx = av_buffer_ref(deviceContext);
    }
     if(avcodec_open2(*inputCodecContext, *inputCodec, nullptr) < 0)
        throw std::runtime_error("Cannot open codec.");
}

void Decoder::initDecoder(std::string file)
{
    auto startInit = std::chrono::steady_clock::now();
    initDecoderParams(&formatContext, &codec, &codecContext, file);
    auto endInit = std::chrono::steady_clock::now();
    std::cout << "Init: " << std::chrono::duration_cast<std::chrono::milliseconds>(endInit-startInit).count() << "ms" << std::endl;
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

Decoder::Decoder(std::string path, bool cpuDecoding)
{
    if(cpuDecoding)
        GPU=false;
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

void Decoder::saveFrame(AVFrame *inputFrame, std::string path)
{ 
    auto start = std::chrono::steady_clock::now();
    AVFrame *frame = inputFrame;
    //std::cerr << av_get_pix_fmt_name(AVPixelFormat(inputFrame->format)) << std::endl;

    if(inputFrame->format == AV_PIX_FMT_CUDA)
    {
        auto swFrame = av_frame_alloc();
        swFrame->format = AV_PIX_FMT_YUV444P;
        av_hwframe_transfer_data(swFrame, inputFrame, 0);
        frame = swFrame;
    }

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
    auto end = std::chrono::steady_clock::now();
    std::cout << "Storing: " << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << "ms" << std::endl;
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
            saveFrame(frame, outPath+"/"+std::to_string(indices.front())+"-proposed.png");//std::to_string(decoded)+".png");
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

    auto startInit = std::chrono::steady_clock::now();
    initDecoderParams(&classicFormatContext, &classicCodec, &classicCodecContext, file);
    auto classicPacket = av_packet_alloc();
    auto classicFrame = av_frame_alloc();
    auto endInit = std::chrono::steady_clock::now();
    std::cout << "Init: " << std::chrono::duration_cast<std::chrono::milliseconds>(endInit-startInit).count() << "ms" << std::endl;

    size_t position {static_cast<size_t>(round(factor*(offsets.size()-2)))};
    //assuming there are no keyframes except for the first one
    size_t counter = -1; 
    bool decoding = true;
    av_read_frame(classicFormatContext, classicPacket);
    auto start = std::chrono::steady_clock::now();
    auto end = std::chrono::steady_clock::now();
    while(decoding)
    {
        if((classicPacket->flags & AV_PKT_FLAG_KEY) == AV_PKT_FLAG_KEY)
            end = std::chrono::steady_clock::now();
        int err=0;
        while(err == 0)
        {
            if((err = avcodec_send_packet(classicCodecContext, classicPacket)) != 0)
                break;
            av_packet_unref(classicPacket);
            err = av_read_frame(classicFormatContext, classicPacket);
            if(err == AVERROR_EOF)
            {
                classicPacket->data=nullptr;
                classicPacket->size = 0;
            }
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
    std::cout << "Decoding time classic seeking: " << std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count() << "ms" << std::endl;
    saveFrame(classicFrame, outPath+"/"+std::to_string(counter)+"-"+std::filesystem::path(file).stem().string()+".png");
    av_packet_free(&classicPacket);
    av_frame_free(&classicFrame);
}

void Decoder::decodeFrameClassicKey(float factor, [[maybe_unused]] enum Interpolation method, std::string file, std::string outPath)
{
    AVFormatContext *classicFormatContext;
    AVCodec *classicCodec;
    AVCodecContext *classicCodecContext;
    
    auto startInit = std::chrono::steady_clock::now();
    initDecoderParams(&classicFormatContext, &classicCodec, &classicCodecContext, file);
    auto classicPacket = av_packet_alloc();
    auto classicFrame = av_frame_alloc();
    auto endInit = std::chrono::steady_clock::now();
    std::cout << "Init: " << std::chrono::duration_cast<std::chrono::milliseconds>(endInit-startInit).count() << "ms" << std::endl;

    size_t position {static_cast<size_t>(round(factor*(offsets.size()-2)))};

    //NOT WORKING
    //AVStream *classicStream = classicFormatContext->streams[0];
    //size_t seekPosition = (position * classicStream->r_frame_rate.den * classicStream->time_base.den) /(classicStream->r_frame_rate.num * classicStream->time_base.num);
    //av_seek_frame(classicFormatContext, 0, seekPosition, AVSEEK_FLAG_ANY);
    //avformat_seek_file(classicFormatContext, -1, position, position, position, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
    for(int i=0; i<position; i++)
        av_read_frame(classicFormatContext, classicPacket);

    av_read_frame(classicFormatContext, classicPacket);
    avcodec_send_packet(classicCodecContext, classicPacket);
    classicPacket->data=nullptr;
    classicPacket->size = 0;
    avcodec_send_packet(classicCodecContext, classicPacket);
    avcodec_receive_frame(classicCodecContext, classicFrame);
    saveFrame(classicFrame, outPath+"/"+std::to_string(position)+"-classicAllKey.png");
    av_packet_free(&classicPacket);
    av_frame_free(&classicFrame);
}

