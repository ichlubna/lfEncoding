extern "C" { 
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}
#include <stdexcept>
#include <zlib.h>
#include <filesystem>
#include <fstream>
#include "encoder.h"
#include "utils.h"

void Encoder::addData(const std::vector<uint8_t> *packetData)
{
    offsets.push_back(data.size()); 
    constexpr size_t HEADER_SIZE{9};
    data.reserve(data.size() + packetData->size()); 
    data.insert(data.end(), packetData->begin()+HEADER_SIZE, packetData->end());
    /*for (int i=0; i<50; i++)
    std::cerr << static_cast<unsigned int>(packetData->at(i)) << " ";
    std::cerr << std::endl << std::endl; 
    std::cerr << packetData->size() <<std::endl;*/
}

Encoder::Encoder(uint32_t refID, std::string refFile) : referenceIndex{refID}, referenceFile{refFile} 
{
}

void Encoder::encodeFrame(std::string file)
{
    PairEncoder newFrame(referenceFile, file);
    addData(newFrame.getFramePacket());
}

void Encoder::save(std::string path)
{
    //lfo contains the offsets into lfp (packet data)
    //the last offset is the index of the reference frame which is stored as first packet in lfp
    offsets.push_back(data.size()-1);
    offsets.push_back(referenceIndex);
    gzFile f = gzopen((path+"offsets.lfo").c_str(), "wb");
    gzwrite(f, reinterpret_cast<uint8_t*>(offsets.data()), offsets.size()*4);
    gzclose(f);
    std::ofstream(path+"packets.lfp", std::ios::binary).write(reinterpret_cast<const char*>(data.data()), data.size()); 

    //TODO mux in the encoding code 
    std::string cmd{"ffmpeg -i "+referenceFile+" -y -c:v libx265 -pix_fmt yuv444p -loglevel error -x265-params \"log-level=error:keyint=60:min-keyint=60:scenecut=0:crf=20\" "+path+"/reference.mkv" };
    system(cmd.c_str());
}

Encoder::FFEncoder::FFEncoder(size_t width, size_t height, AVPixelFormat pixFmt)
{
    std::string codecName = "libx265";
    std::string codecParamsName = "x265-params";
    std::string codecParams = "log-level=error:keyint=60:min-keyint=60:scenecut=0:crf=20";
    codec = avcodec_find_encoder_by_name(codecName.c_str());
    codecContext = avcodec_alloc_context3(codec);
    if(!codecContext)
        throw std::runtime_error("Cannot allocate output context!");
    codecContext->height = height;
    codecContext->width = width;
    codecContext->pix_fmt = pixFmt;
    codecContext->time_base = {1,60};
    av_opt_set(codecContext->priv_data, codecParamsName.c_str(), codecParams.c_str(), 0);
    if(avcodec_open2(codecContext, codec, nullptr) < 0)
        throw std::runtime_error("Cannot open output codec!");
    packet = av_packet_alloc();
}

Encoder::FFEncoder::~FFEncoder()
{
    //avformat_close_input(&formatContext);
    //avformat_free_context(formatContext);
    avcodec_free_context(&codecContext);
    av_packet_free(&packet);
}

AVPacket* Encoder::FFEncoder::retrievePacket()
{
    bool waitForPacket = true;
    while(waitForPacket)
    {
        int err = avcodec_receive_packet(codecContext, packet);
        if(err == AVERROR_EOF || err == AVERROR(EAGAIN))
            return nullptr;
        else if(err < 0)
            throw std::runtime_error("Cannot receive packet");
        waitForPacket = false;
    }
    return packet;
}

void Encoder::FFEncoder::encodeFrame(AVFrame *frame)
{
    avcodec_send_frame(codecContext, frame);
}

Encoder::PairEncoder::Frame::Frame(std::string file)
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
    frame = av_frame_alloc();
    packet = av_packet_alloc();
    av_read_frame(formatContext, packet);
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
}

Encoder::PairEncoder::Frame::~Frame()
{
    avformat_close_input(&formatContext);
    avformat_free_context(formatContext);
    avcodec_free_context(&codecContext);
    av_frame_free(&frame);
    av_packet_free(&packet);
}

void Encoder::PairEncoder::encode()
{
    Frame reference(referenceFile); 
    Frame frame(frameFile); 
    auto referenceFrame = reference.getFrame();

    FFEncoder encoder(referenceFrame->width, referenceFrame->height, outputPixelFormat); 

    auto convertedReference = Utils::ConvertedFrame(reference.getFrame(), outputPixelFormat);
    auto convertedFrame = Utils::ConvertedFrame(frame.getFrame(), outputPixelFormat);
    
    encoder << convertedReference.getFrame();
    AVFrame *convertedFrameRaw = convertedFrame.getFrame();
    convertedFrameRaw->key_frame = 0;
    encoder << convertedFrameRaw;
    encoder << nullptr;

    std::vector<uint8_t> *buffer = &referencePacket; 
    AVPacket *packet;
    for(int i=0; i<2; i++)
    {
        encoder >> &packet; 
        if(!packet)
            throw std::runtime_error("Cannot receieve packet!");
        buffer->insert(buffer->end(), &packet->data[0], &packet->data[packet->size]);
        buffer = &framePacket;
    }
}
