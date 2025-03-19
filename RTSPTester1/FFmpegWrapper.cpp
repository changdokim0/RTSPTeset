#include "FFmpegWrapper.h"
#include <iostream>

FFmpegWrapper::FFmpegWrapper()
    : codecContext(nullptr), codec(nullptr), packet(nullptr), frame(nullptr), hw_device_ctx(nullptr) {
}

FFmpegWrapper::~FFmpegWrapper() {
    Cleanup();
}

bool FFmpegWrapper::Initialize(CODEC_TYPE codec_type) {
    codec = (AVCodec*)avcodec_find_decoder(codec_type == CODEC_TYPE::H264 ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC);
    if (!codec) {
        std::cerr << "Codec not found." << std::endl;
        return false;
    }
    codecType = codec_type;
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        std::cerr << "Could not allocate codec context." << std::endl;
        return false;
    }

    if (InitHWDecoder(codecContext, AV_HWDEVICE_TYPE_DXVA2) < 0) {
        std::cerr << "Failed to initialize hardware decoder." << std::endl;
        return false;
    }

    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open codec." << std::endl;
        return false;
    }

    packet = av_packet_alloc();
    frame = av_frame_alloc();
    return true;
}

bool FFmpegWrapper::InitHWDecoder(AVCodecContext* ctx, const enum AVHWDeviceType type) {
    int err = av_hwdevice_ctx_create(&hw_device_ctx, type, nullptr, nullptr, 0);
    if (err < 0) {
        std::cerr << "Failed to create specified HW device." << std::endl;
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    return 0;
}

enum AVPixelFormat FFmpegWrapper::GetHWFormat(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
    for (const enum AVPixelFormat* p = pix_fmts; *p != -1; p++) {
        if (*p == AV_PIX_FMT_CUDA) {
            return *p;
        }
    }
    std::cerr << "Failed to get HW surface format." << std::endl;
    return AV_PIX_FMT_NONE;
}

bool FFmpegWrapper::ReceiveFrame(uint8_t* data, int size, YUVData& yuvData) {
    if (!data || size <= 0) {
        std::cerr << "Invalid input data." << std::endl;
        return false;
    }

    av_packet_unref(packet);
    av_new_packet(packet, size);
    memcpy(packet->data, data, size);

    int response = avcodec_send_packet(codecContext, packet);
    if (response < 0) {
        std::cerr << "Error sending packet for decoding." << std::endl;
        return false;
    }

    response = avcodec_receive_frame(codecContext, frame);
    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
        //continue;
    }
    else if (response < 0) {
        std::cerr << "Error during decoding." << std::endl;
        return false;
    }

    return true;
}
bool FFmpegWrapper::GetFrame(YUVData& yuvData) {
    sw_frame = av_frame_alloc();
    if (!sw_frame) {
        std::cerr << "Cannot allocate software frame." << std::endl;
        return false;
    }

    if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0) {
        std::cerr << "Error transferring the data to system memory." << std::endl;
        av_frame_free(&sw_frame);
        return false;
    }
    
    yuvData.width = sw_frame->width;
    yuvData.height = sw_frame->height;
    yuvData.ySize = sw_frame->linesize[0] * sw_frame->height;
    yuvData.uSize = sw_frame->linesize[1] * (sw_frame->height / 2);
    yuvData.vSize = sw_frame->linesize[1] * (sw_frame->height / 2); // NV12 format
    
    yuvData.yData = std::make_unique<uint8_t[]>(yuvData.ySize);
    yuvData.uData = std::make_unique<uint8_t[]>(yuvData.uSize);
    yuvData.vData = std::make_unique<uint8_t[]>(yuvData.vSize);
    
    for (int i = 0; i < sw_frame->height; i++) {
        memcpy(yuvData.yData.get() + i * sw_frame->linesize[0], sw_frame->data[0] + i * sw_frame->linesize[0], sw_frame->width);
    }
    
    for (int i = 0; i < sw_frame->height / 2; i++) {
        for (int j = 0; j < sw_frame->width / 2; j++) {
            yuvData.uData[i * (sw_frame->width / 2) + j] = sw_frame->data[1][i * sw_frame->linesize[1] + j * 2];
            yuvData.vData[i * (sw_frame->width / 2) + j] = sw_frame->data[1][i * sw_frame->linesize[1] + j * 2 + 1];
        }
    }
    av_frame_free(&sw_frame);
}
void FFmpegWrapper::Cleanup() {
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);
    av_buffer_unref(&hw_device_ctx);
}