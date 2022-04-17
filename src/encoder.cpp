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
    //lfo contains the offsets into lfp (packet data) - the last offset is the index of the reference frame which is stored as first packet in lfp
    offsets.push_back(referenceIndex);
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

Encoder::PairEncoder::ConvertedFrame::ConvertedFrame(Frame *inputFrame, AVPixelFormat format)
{
    frame = av_frame_alloc();
    auto input = inputFrame->getFrame();
    frame->width = input->width;
    frame->height = input->height;
    frame->format = format;
    av_frame_get_buffer(frame, 24);
/*  av_frame_copy_props(frame, input);
    frame->linesize[0] = input->width;
    frame->linesize[1] = input->width;
    frame->linesize[2] = input->width;
    frame->format =format;
    av_image_alloc(frame->data, frame->linesize, input->width, input->height, format, 1);
    av_image_fill_arrays(frame->data, frame->linesize, nullptr, format, input->width,input->height, 1); */
    auto swsContext = sws_getContext(   input->width, input->height, static_cast<AVPixelFormat>(input->format),
                                        input->width, input->height, format, SWS_BICUBIC, nullptr, nullptr, nullptr);
    if(!swsContext)
        throw std::runtime_error("Cannot get conversion context!");
    sws_scale(swsContext, input->data, input->linesize, 0, input->height, frame->data, frame->linesize);  
}

Encoder::PairEncoder::ConvertedFrame::~ConvertedFrame()
{
    av_frame_free(&frame);
}

void Encoder::PairEncoder::encode()
{
    Frame reference(referenceFile); 
    Frame frame(referenceFile); 
    auto referenceFrame = reference.getFrame();

    std::string codecName = "libx265";
    std::string codecParamsName = "x265-params";
    std::string codecParams = "log-level=error:keyint=60:min-keyint=60:scenecut=0:crf=20";
    auto codec = avcodec_find_encoder_by_name(codecName.c_str());
    auto codecContext = avcodec_alloc_context3(codec);
    if(!codecContext)
        throw std::runtime_error("Cannot allocate output context!");
    codecContext->height = referenceFrame->height;
    codecContext->width = referenceFrame->width;
    codecContext->pix_fmt = outputPixelFormat;
    codecContext->time_base = {1,1};
    av_opt_set(codecContext->priv_data, codecParamsName.c_str(), codecParams.c_str(), 0);
    if(avcodec_open2(codecContext, codec, nullptr) < 0)
        throw std::runtime_error("Cannot open output codec!");

    auto convertedReference = ConvertedFrame(&reference, outputPixelFormat);
    auto convertedFrame = ConvertedFrame(&frame, outputPixelFormat);
    avcodec_send_frame(codecContext, convertedReference.getFrame());
    avcodec_send_frame(codecContext, convertedFrame.getFrame());
    avcodec_send_frame(codecContext, nullptr);
    bool waitForPacket = true;
    auto packet = av_packet_alloc();
    std::vector<uint8_t> *buffer = &referencePacket;
    int i=0;
    while(waitForPacket || i<2)
    {
        int err = avcodec_receive_packet(codecContext, packet);
        if(err == AVERROR_EOF || err == AVERROR(EAGAIN))
            waitForPacket = false;                
        else if(err < 0)
            throw std::runtime_error("Cannot receive packet");
        i++;
        buffer->insert(buffer->end(), &packet->data[0], &packet->data[packet->size]);
        buffer = &framePacket;  
    }
    avcodec_free_context(&codecContext);
    av_packet_free(&packet);
}
