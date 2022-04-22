extern "C" { 
#include <libswscale/swscale.h>
}
#include <stdexcept>
#include "utils.h"

Utils::ConvertedFrame::~ConvertedFrame()
{
    av_frame_free(&outputFrame);
}

Utils::ConvertedFrame::ConvertedFrame(const AVFrame *inputFrame, AVPixelFormat format)
{
    outputFrame = av_frame_alloc();
    outputFrame->width = inputFrame->width;
    outputFrame->height = inputFrame->height;
    outputFrame->format = format;
    av_frame_get_buffer(outputFrame, 24);
    auto swsContext = sws_getContext(   inputFrame->width, inputFrame->height, static_cast<AVPixelFormat>(inputFrame->format),
                                        inputFrame->width, inputFrame->height, format, SWS_BICUBIC, nullptr, nullptr, nullptr);
    if(!swsContext)
        throw std::runtime_error("Cannot get conversion context!");
    sws_scale(swsContext, inputFrame->data, inputFrame->linesize, 0, inputFrame->height, outputFrame->data, outputFrame->linesize);  
}
