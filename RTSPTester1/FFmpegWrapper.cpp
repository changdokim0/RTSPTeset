#include "FFmpegWrapper.h"
#include <iostream>

FFmpegWrapper::FFmpegWrapper()
    : codecContext(nullptr), codec(nullptr), packet(nullptr), frame(nullptr) {
}

FFmpegWrapper::~FFmpegWrapper() {
    //Cleanup();
}

bool FFmpegWrapper::Initialize(const char* codecName) {
    //avcodec_register_all(); // ��� �ڵ� ���

    // �ڵ� ã��
    codec = (AVCodec*)avcodec_find_decoder(strcmp(codecName, "h264") == 0 ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC);
    if (!codec) {
        std::cerr << "Codec not found." << std::endl;
        return false;
    }

    // �ڵ� ���ؽ�Ʈ �ʱ�ȭ
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        std::cerr << "Could not allocate codec context." << std::endl;
        return false;
    }

    // �ڵ� ����
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open codec." << std::endl;
        return false;
    }

    packet = av_packet_alloc();
    frame = av_frame_alloc();
    return true;
}

bool FFmpegWrapper::ReceiveFrame(uint8_t* data, int size, YUVData& yuvData) {
    if (!data || size <= 0) {
        std::cerr << "Invalid input data." << std::endl;
        return false;
    }

    // ��Ŷ�� ������ ����
    av_packet_unref(packet);
    av_new_packet(packet, size);
    memcpy(packet->data, data, size);

    // ������ ���ڵ�
    int response = avcodec_send_packet(codecContext, packet);
    if (response < 0) {
        std::cerr << "Error sending packet for decoding." << std::endl;
        return false;
    }

    while (response >= 0) {
        response = avcodec_receive_frame(codecContext, frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break; // �� �̻� �������� ����
        }
        else if (response < 0) {
            std::cerr << "Error during decoding." << std::endl;
            return false;
        }

        // YUV ������ �Ҵ� �� ����
        yuvData.ySize = frame->width * frame->height;
        yuvData.uSize = (frame->width / 2) * (frame->height / 2);
        yuvData.vSize = yuvData.uSize;
        yuvData.height = frame->height;
        yuvData.width = frame->width;

        // �޸� �Ҵ�
        yuvData.yData = std::make_unique<uint8_t[]>(yuvData.ySize);
        yuvData.uData = std::make_unique<uint8_t[]>(yuvData.uSize);
        yuvData.vData = std::make_unique<uint8_t[]>(yuvData.vSize);

        // YUV ������ ����
        memcpy(yuvData.yData.get(), frame->data[0], yuvData.ySize); // Y plane
        memcpy(yuvData.uData.get(), frame->data[1], yuvData.uSize); // U plane
        memcpy(yuvData.vData.get(), frame->data[2], yuvData.vSize); // V plane

        std::cout << "Received frame " << frame->pts << " with size Y: " << yuvData.ySize
            << ", U: " << yuvData.uSize << ", V: " << yuvData.vSize << std::endl;
    }

    return true;
}

void FFmpegWrapper::Cleanup() {
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);
}