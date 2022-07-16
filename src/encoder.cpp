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
}

Encoder::Encoder(uint32_t refID, std::string refFile, size_t inCrf) : referenceIndex{refID}, referenceFile{refFile}, crf{inCrf} 
{
}

void Encoder::encodeClassic(std::set<std::filesystem::path> *sortedFiles, std::string path, size_t keyInterval) const
{
    PairEncoder::Frame check(*sortedFiles->begin());
    auto rawFrame = check.getFrame();
    FFEncoder encoder(rawFrame->width, rawFrame->height, outputPixelFormat, crf, keyInterval);
    FFMuxer muxer(path, encoder.getCodecContext());
    AVPacket *packet;
    size_t sent{0};
    size_t counter{0};
    

    for(auto const &file : *sortedFiles)
    {    
        PairEncoder::Frame frame(file);
        auto converted = Utils::ConvertedFrame(frame.getFrame(), outputPixelFormat);
        auto rawFrame = converted.getFrame();
        rawFrame->key_frame = 1;
        encoder << rawFrame;// converted.getFrame();
        sent++;
        if(sent == sortedFiles->size())
            encoder << nullptr;

        while(true)
        {
            encoder >> &packet;
            if(packet == nullptr)
                break;
            packet->duration = 1;
            counter++;
            packet->dts = packet->pts = counter;
            muxer << packet;
        }
    }
}

void Encoder::encodeReference(std::string path)
{
    PairEncoder::Frame frame(referenceFile);
    auto rawFrame = frame.getFrame();
    FFEncoder encoder(rawFrame->width, rawFrame->height, outputPixelFormat, crf);
    auto converted = Utils::ConvertedFrame(rawFrame, outputPixelFormat);

    encoder << converted.getFrame();
    encoder << nullptr;
    AVPacket *packet;
    encoder >> &packet;

    packet->duration = 1;
    packet->dts = 0;
    packet->pts = 0;

    FFMuxer muxer(path+"/reference.ts", encoder.getCodecContext());
    muxer << packet;
    muxer.finish();
}

void Encoder::encodeFrame(std::string file)
{
    PairEncoder newFrame(referenceFile, file, crf);
    addData(newFrame.getFramePacket());
}

void Encoder::save(std::string path)
{
    //lfo contains the offsets into lfp (packet data)
    //the last offset is the index of the reference frame
    offsets.push_back(data.size());
    offsets.push_back(referenceIndex);
    gzFile f = gzopen((path+"/offsets.lfo").c_str(), "wb");
    gzwrite(f, reinterpret_cast<uint8_t*>(offsets.data()), offsets.size()*4);
    gzclose(f);
    std::ofstream(path+"/packets.lfp", std::ios::binary).write(reinterpret_cast<const char*>(data.data()), data.size()); 

    encodeReference(path);    
}

Encoder::FFMuxer::FFMuxer(std::string fileName, const AVCodecContext *encoderCodecContext)
{
    avformat_alloc_output_context2(&muxerFormatContext, nullptr, nullptr, fileName.c_str());
    if(!muxerFormatContext)
        throw std::runtime_error("Cannot allocate output context!");
    auto stream = avformat_new_stream(muxerFormatContext, nullptr);
    avcodec_parameters_from_context(stream->codecpar, encoderCodecContext);
    stream->time_base = encoderCodecContext->time_base;
    if(avio_open(&muxerFormatContext->pb, fileName.c_str(), AVIO_FLAG_WRITE) < 0)
        throw std::runtime_error("Cannot open the output file");
    if(avformat_write_header(muxerFormatContext, nullptr) < 0)
        throw std::runtime_error("Cannot write the header");
}

void Encoder::FFMuxer::writePacket(AVPacket *packet)
{
    if(av_write_frame(muxerFormatContext, packet) < 0)
        throw std::runtime_error("Cannot write packet!");
}

void Encoder::FFMuxer::finish()
{
    finished = true;
    av_write_trailer(muxerFormatContext);
}

Encoder::FFMuxer::~FFMuxer()
{
    if(!finished)
        finish();
    avformat_free_context(muxerFormatContext); 
}

Encoder::FFEncoder::FFEncoder(size_t width, size_t height, AVPixelFormat pixFmt, size_t crf, size_t keyInterval)
{
    std::string codecName = "libx265";
    std::string codecParamsName = "x265-params";
    std::string codecParams = "log-level=error:keyint="+std::to_string(keyInterval)+":min-keyint="+std::to_string(keyInterval)+":scenecut=0:crf="+std::to_string(crf);
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

    FFEncoder encoder(referenceFrame->width, referenceFrame->height, outputPixelFormat, crf); 

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
