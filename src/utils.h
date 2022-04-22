extern "C" { 
#include <libavformat/avformat.h>
}

class Utils
{
    public:
    class ConvertedFrame
    {
        public:
        ConvertedFrame(const AVFrame *frame, AVPixelFormat format);
        ~ConvertedFrame();
        const AVFrame* getFrame() const { return outputFrame;}

        private:
        AVFrame *outputFrame;
        struct SwsContext *swsContext;
    };
};
