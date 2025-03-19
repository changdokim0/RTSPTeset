#pragma once

#include "Pnx++/Media/MediaDefine.h"
#include <memory>
#include <functional>

namespace Pnx
{
namespace Media
{

enum class MediaSourceType
{
    Unknown = 0,
    RtpFrame,
    StdFile,
    PnxExportFile,
};

struct MediaSourceFrame
{
    MediaTimeType   timeType = MediaTimeType::UtcMsec;
    int64_t         pts = 0;
    uint32_t        sequence = 0;   /// continous increment while continous media
    unsigned char   cSeq = 0;       /// CSeq and RTP extension when playback

    explicit MediaSourceFrame(const unsigned char *dataPtr = 0, const size_t dataSize = 0)
    {
        if (dataSize > 0 && dataPtr != nullptr) {
            m_dataPtr = new unsigned char[dataSize];
            m_dataSize = dataSize;
            std::copy(dataPtr, dataPtr+dataSize, m_dataPtr);
        }
    }
    virtual ~MediaSourceFrame()
    {
        if (m_dataPtr != nullptr)
            delete[] m_dataPtr;

        m_dataPtr = nullptr;
        m_dataSize = 0;
    }

    size_t dataSize() const
    {
        return m_dataSize;
    }

    unsigned char * data() const
    {
        return m_dataPtr;
    }

    virtual MediaType mediaType() const {
        return MediaType::Unknown;
    }
    virtual MediaSourceType sourceType() const {
        return MediaSourceType::Unknown;
    }

protected:
    size_t          m_dataSize = 0;
    unsigned char * m_dataPtr = nullptr;
};


struct VideoSourceFrame : MediaSourceFrame
{
    VideoFrameType  frameType = VideoFrameType::Unknown;
    VideoCodecType  codecType = VideoCodecType::Unknown;
    Resolution      resolution;
    float           frameRate = 0;
    int64_t         dts = -1; // can be used when pts does not same to dts

    /** https://www.onvif.org/specs/stream/ONVIF-Streaming-Spec.pdf **/
    // C : CEDT & 0X80, Clean point
    // E : CEDT & 0X40, End of a contiguous section of recording
    // D : CEDT & 0X20, Discontinuity in transmission
    // T : CEDT & 0X10, Terminal frame on plackback of a tack.
    // P : CEDT & 0X08, End of a GoV (Phoenix-Play)
    char CEDT = 0;

    bool cFlag() const {return CEDT & 0X80;}
    bool eFlag() const {return CEDT & 0X40;}
    bool dFlag() const {return CEDT & 0X20;}
    bool tFlag() const {return CEDT & 0X10;}
    bool pFlag() const {return CEDT & 0X08;}
    void unsetCflag() { CEDT &= ~0x80;}
    void setCflag() { CEDT |= 0x80;}
    void setEflag() { CEDT |= 0x40;}
    void setDflag() { CEDT |= 0x20;}
    void setTflag() { CEDT |= 0x10;}
    void setPflag() { CEDT |= 0x08;}

    explicit VideoSourceFrame(const unsigned char *dataPtr = nullptr,
                              const size_t dataSize = 0)
        : MediaSourceFrame(dataPtr, dataSize)
    {}

    MediaType mediaType() const override
    {
        return MediaType::Video;
    }
};
typedef std::shared_ptr<VideoSourceFrame>        VideoSourceFramePtr;

struct VideoSourceFrameList: VideoSourceFrame
{
    VideoSourceFrameList()
    {}
    MediaType mediaType() const override
    {
        return MediaType::VideoList;
    }
    void setPresentationFrame(VideoSourceFramePtr frame)
    {
        if (presentationFrame) {
            presentationFrame->unsetCflag();
        }
        if (frame) {
            frame->setCflag();
        }
        presentationFrame = frame;
    }

    void clear()
    {
        presentationFrame = nullptr;
        frames.clear();
    }
    VideoSourceFramePtr presentationFrame;
    std::vector<VideoSourceFramePtr> frames;
};

struct AudioSourceFrame: MediaSourceFrame
{
    AudioCodecType codecType = AudioCodecType::Unknown;
    int audioChannels = 0;      // number of audio channels (1:mono, 2:stereo)
    int audioSampleRate = 0;    // Samples per seconds (Hz: 16000, 32000, 44100...)
    int audioBitPerSample = 0;  // Bit per Sample (bits)
    int audioBitrate = 0;       // Bitrate (bits)

    explicit AudioSourceFrame(const unsigned char *dataPtr = nullptr,
                              const size_t dataSize = 0)
        : MediaSourceFrame(dataPtr, dataSize)
    {}

    MediaType mediaType() const override
    {
        return MediaType::Audio;
    }
};

struct MetaDataSourceFrame: MediaSourceFrame
{
    explicit MetaDataSourceFrame(const unsigned char *dataPtr = nullptr,
                                 const size_t dataSize = 0)
        : MediaSourceFrame(dataPtr, dataSize)
    {}

    MediaType mediaType() const override
    {
        return MediaType::MetaData;
    }
};

struct MetaImageSourceFrame: MediaSourceFrame
{
    Resolution      resolution;
    explicit MetaImageSourceFrame(const unsigned char *dataPtr = nullptr,
                                 const size_t dataSize = 0)
        : MediaSourceFrame(dataPtr, dataSize)
    {}

    MediaType mediaType() const override
    {
        return MediaType::MetaImage;
    }
};


typedef std::shared_ptr<MediaSourceFrame>        MediaSourceFramePtr;
typedef std::shared_ptr<AudioSourceFrame>        AudioSourceFramePtr;
typedef std::shared_ptr<MetaDataSourceFrame>     MetaDataSourceFramePtr;
typedef std::shared_ptr<MetaImageSourceFrame>    MetaImageSourceFramePtr;
typedef std::shared_ptr<VideoSourceFrameList>    VideoSourceFrameListPtr;

typedef std::function <void(const MediaSourceFramePtr&)>       MediaSourceFrameHandler;
typedef std::function <void(const VideoSourceFramePtr&)>       VideoSourceFrameHandler;
typedef std::function <void(const AudioSourceFramePtr&)>       AudioSourceFrameHandler;
typedef std::function <void(const MetaDataSourceFramePtr&)>    MetaDataSourceFrameHandler;
typedef std::function <void(const MetaImageSourceFramePtr&)>   MetaImageSourceFrameHandler;

}
}
