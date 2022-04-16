#include <zlib.h>
#include <libavutil/opt.h>
#include <filesystem>
#include <fstream>
#include "encoder.h"

void Encoder::addData(const std::vector<uint8_t> *packetData)
{
    offsets.push_back(data.size()); 
    data.reserve(data.size() + packetData->size()); 
    data.insert(data.end(), packetData->begin(), packetData->end());
}

Encoder::Encoder(uint32_t refID, std::string refFile) : referenceIndex{refID}, referenceFile{refFile} 
{
    PairEncoder referenceFrame(refFile, refFile);
    addData(referenceFrame.getReferencePacket()); 
}

void Encoder::encodeFrame(std::string file)
{
    PairEncoder newFrame(referenceFile, file);
    addData(newFrame.getFramePacket());
}

void Encoder::save(std::string path)
{
    gzFile f = gzopen((path+"offsets.lfo").c_str(), "wb");
    gzwrite(f, offsets.data(), offsets.size());
    gzclose(f);
    std::ofstream(path+"packets.lfp", std::ios::binary).write(reinterpret_cast<const char*>(data.data()), data.size()); 
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
    auto *packet = av_packet_alloc();
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
    Frame frame(referenceFile); 
    auto referenceFrame = reference.getFrame();
    auto frameFrame = frame.getFrame();
    
    auto tempFile = std::filesystem::temp_directory_path().string()+"temp.mkv";

    AVFormatContext *formatContext;
    avformat_alloc_output_context2(&formatContext, nullptr, nullptr, tempFile.c_str());
    AVStream *stream = avformat_new_stream(formatContext, NULL);
    std::string codecName = "libx265";
    std::string codecParamsName = "x265-params";
    std::string codecParams = "keyint=60:min-keyint=60:scenecut=0:crf=20";
    auto codec = avcodec_find_encoder_by_name(codecName.c_str());
    auto codecContext = avcodec_alloc_context3(codec);
    av_opt_set(codecContext->priv_data, codecParamsName.c_str(), codecParams.c_str(), 0);
    codecContext->height = referenceFrame->height;
    codecContext->width = referenceFrame->width;
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P ;

    avcodec_open2(codecContext,codec, nullptr);
    avcodec_parameters_from_context(stream->codecpar, codecContext);
    avcodec_send_frame(codecContext, referenceFrame);
    avcodec_send_frame(codecContext, frameFrame);
    avcodec_send_frame(codecContext, nullptr);
    bool waitForPacket = true;
    auto packet = av_packet_alloc();
    std::vector<uint8_t> *buffer = &referencePacket;
    while(waitForPacket)
    {
        int err = avcodec_receive_packet(codecContext, packet);
        if(err == AVERROR_EOF || err == AVERROR(EAGAIN))
            waitForPacket = false;                
        else if(err < 0)
            throw std::runtime_error("Cannot receive packet");

       buffer->insert(referencePacket.end(), &packet->data[0], &packet->data[packet->size]);
       buffer = &framePacket;  
    }
 
    avformat_free_context(formatContext);
    avcodec_free_context(&codecContext);
    av_packet_free(&packet);
}
