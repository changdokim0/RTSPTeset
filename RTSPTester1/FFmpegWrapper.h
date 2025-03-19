#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <cstring>
#include <optional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
struct YUVData {
    int width = 0, height = 0;

    std::unique_ptr<uint8_t[]> yData; // Y plane
    int ySize; // Y plane size

    std::unique_ptr<uint8_t[]> uData; // U plane
    int uSize; // U plane size

    std::unique_ptr<uint8_t[]> vData; // V plane
    int vSize; // V plane size
};

class FFmpegWrapper {
public:
    FFmpegWrapper();
    ~FFmpegWrapper();


    bool Initialize(const char* codecName);
    bool ReceiveFrame(uint8_t* data, int size, YUVData& yuvData);
    void Cleanup();

private:
    AVCodecContext* codecContext;
    AVCodec* codec;
    AVPacket* packet;
    AVFrame* frame;
};