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

enum CODEC_TYPE {
	H264,
	H265,
	MPEG4,
	VP8,
	VP9,
	AV1,
	GIF,
    Unknown
};

class FFmpegWrapper {
public:
    FFmpegWrapper();
    ~FFmpegWrapper();


    bool Initialize(CODEC_TYPE codec_type);
    bool ReceiveFrame(uint8_t* data, int size, YUVData& yuvData);
    void Cleanup();
	CODEC_TYPE GetCodecType() { return codecType; }
    bool GetFrame(YUVData& yuvData);

private:
    AVCodecContext* codecContext;
    AVCodec* codec;
    AVPacket* packet;
    AVFrame* frame;
    AVBufferRef* hw_device_ctx;
	CODEC_TYPE codecType;
	AVFrame* sw_frame = nullptr;

    bool InitHWDecoder(AVCodecContext* ctx, const enum AVHWDeviceType type);
    static enum AVPixelFormat GetHWFormat(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts);
};