#pragma once

#include <functional>
#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <limits>

namespace Pnx
{
namespace Media
{

enum class MediaType
{
    Unknown,
    Video,
    Audio,
    MetaData,
    MetaImage,
    VideoList,
    Image
};

enum class PixelFormat
{
    Unknown,
    YUV420P,
    YUVJ420P,
    NV12,
    BGRA
};

inline std::string PixelFormatString(PixelFormat format)
{
    switch (format) {
    case PixelFormat::YUV420P: return "YUV420P";
    case PixelFormat::YUVJ420P: return "YUVJ420P";
    case PixelFormat::NV12: return "NV12";
    case PixelFormat::BGRA: return "BGRA";
    default: return "Unknown";
    }
}


enum class VideoCodecType
{
    Unknown = 0,
    H265,
    H264,
    MJPEG,
    MPEG4,
    VP8,
    VP9,
    AV1,
    GIF
};

enum class VideoFrameType
{
    Unknown = 0,
    I_FRAME,
    P_FRAME
};

enum class AudioCodecType
{
    Unknown = 0,
    G711U,
    G711A,
    G723,
    G726,
    AAC
};

enum class ErrorCodes
{
    NoError = 0,
    InvalidRequest,     // wrong requst
    InvalidRange,       // invalid search range
    Unauthorized,       // unauthorized
    NetworkError,       // stream : network error
    Timedout,           // stream : network error due to timedout
    UserFull,           // stream : user full
    Forbidden,          // stream : user forbidden
    OptionNotSupported, // stream : Option Not Supported
    ExtraNetworkBadStatus,  // stream : other bad status code
    FileOpenFailed,     // file : open failed
    InvalidFile,        // file : not support file
    FileWriteFailed,    // file : write failed
};
enum class DewarpViewMode
{
    Regular = 0,
    Panoramic
};

enum class DewarpLocation
{
    Ceiling = 0,
    Ground,
    Wall
};

// same as AV_NOPTS_VALUE of FFMPEG
#define UNDEFINED_PTS_VALUE  ((int64_t)UINT64_C(0x8000000000000000))
//constexpr int64_t kMediaEndOfPts = std::numeric_limits<int64_t>::max();

inline DewarpLocation DewarpLocationEnum(const std::string mode) {
    if (mode == "Ceiling") {
        return DewarpLocation::Ceiling;
    }
    if (mode == "Floor") {
        return DewarpLocation::Ground;
    }
    if (mode == "Wall") {
        return DewarpLocation::Wall;
    }
    return DewarpLocation::Ground;
}

inline std::string DewarpLocationString(const DewarpLocation location){
    switch(location) {
        case DewarpLocation::Ground:
            return "Floor";
        case DewarpLocation::Wall:
            return "Wall";
        default:
            return "Ceiling";
    }
}

struct DewarpLensInfo
{
    std::string rpl;
    DewarpLocation location = DewarpLocation::Ceiling;
    float rotation = 0.0f;
};

enum class PlayFrameType {
    All,
    Intra
};

enum class PlayQualityType {
    Primary,
    Secondary,
    Auto        // follow stream url
};

enum class PlayStreamType {
    Live,
    Playback
};

struct MediaPlayParams
{
    // use pts or timeStr format
    std::optional<int64_t> startPts;        // start time (msec)
    std::optional<int64_t> endPts;          // end time (msec)
    std::optional<bool> paused;             // if paused true, scale value should be ignored.
    std::optional<bool> resume;             // if resume true & previous paused state is true, other values are ignored
    std::optional<int>  zeroScaleDirection; // when paused is true, Direction of finding key-frame with startPts, 0 means both side only valid if fastSeek is true
    std::optional<float> scale;             // >0 means forward, 0 means paused seek, < means backward
    std::optional<PlayFrameType> frameType; // intra, full frame, any
    std::optional<bool> rateControl;        // false if stream is used for export
    std::optional<bool> qualityControl;     // phoenix-play only, false if stream should not be changed automatically (for backup)

    std::optional<PlayStreamType> streamType;   // only valid for phoenix-play
    std::optional<PlayQualityType> quality;     // only valid for phoenix-play
    std::optional<int> searchPeriod;          // only valid for phoenix-play

    bool immediate = true;                  // true means discard previous request

    bool isBackwardZeroSeek() const {
        return zeroScaleDirection.value_or(-1) <= 0 ? true : false;
    }
};

struct MediaPlayStatus
{
    MediaPlayStatus() {};
    MediaPlayStatus(const PlayStreamType st) : streamType(st) {}

    PlayStreamType streamType = PlayStreamType::Playback;
    PlayQualityType quality = PlayQualityType::Auto;

    int64_t startPts = UNDEFINED_PTS_VALUE;
    int64_t endPts = UNDEFINED_PTS_VALUE;
    bool    paused = false;
    float   scale = 1.0f; /* Do not set to 0 scale, use paused */
    int     searchPeriod = -1;
    PlayFrameType frameType = PlayFrameType::All;
    int zeroScaleDirection = -1;

    void updateStatus(const MediaPlayParams& params)
    {
        if (params.streamType) {
            streamType = params.streamType.value();
        }
        if (params.quality) {
            quality = params.quality.value();
        }
        if (params.resume) {
            paused = !params.resume.value();
        }
        else if (params.paused) {
            paused = params.paused.value();
            zeroScaleDirection = params.zeroScaleDirection.value_or(-1);
        }

        if (params.startPts) {
            startPts = params.startPts.value();
        }
        if (params.endPts) {
            endPts = params.endPts.value();
        }
        if (params.scale) {
            scale = params.scale.value();
            if (!params.resume.has_value() &&
                !params.paused.has_value() && scale != 0.0f) {
                paused = false;
            }
        }
        if (params.frameType) {
            frameType = params.frameType.value();
        }

        if (params.searchPeriod) {
            searchPeriod = params.searchPeriod.value();
        }
        else {
            searchPeriod = -1;
        }
    }

    bool isZeroSpeed() const {
        return (paused || scale == 0.0f);
    }
    bool isIntraOnly() const {
        return frameType == PlayFrameType::Intra;
    }
    bool isLive() const {
        return streamType == PlayStreamType::Live;
    }
    bool enableAudioDecoding() const {
        return isLive() || (!isZeroSpeed() && scale == 1.0f && !isIntraOnly());
    }
    bool isBackwardZeroSeek() const {
        return zeroScaleDirection <= 0 ? true : false;
    }
};


struct MediaOpenParams
{
    bool video = true;
    bool audio = true;
    bool metaData = true;
    bool metaImage = true;

    MediaPlayParams playParams;
};

struct MediaSourceResponse
{
    const bool isFailed() const
    {
        return (errorCode != ErrorCodes::NoError);
    }
    const bool isSuccess() const
    {
        return !isFailed();
    }
    const bool isNetworkError() const
    {
        return (errorCode == Media::ErrorCodes::NetworkError ||
                errorCode == Media::ErrorCodes::Timedout);
    }
    const bool isUnauthorized() const
    {
        return (errorCode == Media::ErrorCodes::Unauthorized);
    }
    const bool isInvalidRange() const
    {
        return (errorCode == Media::ErrorCodes::InvalidRange);
    }

    ErrorCodes errorCode = ErrorCodes::NoError;
    unsigned char cSeq = 0;
};
typedef std::shared_ptr<MediaSourceResponse>  MediaSourceResponseSharedPtr;
typedef std::function <void(const MediaSourceResponseSharedPtr&)>  MediaSourceResponseHandler;


inline std::string VideoCodecTypeString(VideoCodecType vcodec)
{
    switch (vcodec) {
    case VideoCodecType::H264: return "H.264";
    case VideoCodecType::H265: return "H.265";
    case VideoCodecType::MJPEG: return "MJPEG";
    case VideoCodecType::MPEG4: return "MPEG4";
    case VideoCodecType::VP8: return "VP8";
    case VideoCodecType::VP9: return "VP9";
    case VideoCodecType::AV1: return "AV1";
    case VideoCodecType::GIF: return "GIF";
    default: return "UNKNOWN";
    }
}

inline std::string AudioCodecTypeString(AudioCodecType acodec)
{
    switch (acodec) {
    case AudioCodecType::G711U: return "G.711U";
    case AudioCodecType::G711A: return "G.711A";
    case AudioCodecType::G723: return "G.723";
    case AudioCodecType::G726: return "G.726";
    case AudioCodecType::AAC: return "AAC";
    default: return "UNKNOWN";
    }
}



enum class MediaTimeType
{
    UtcMsec = 0,     // the number of milliseconds since 1970-01-01T00:00:00 UTC
    PlayTimeMsec,
};

struct PointF {
    PointF(double _x = 0, double _y = 0)
        : x(_x), y(_y)
    {}
    double x = 0;
    double y = 0;
};

struct RectF {
    RectF(double _x = 0, double _y = 0, double _w = 1, double _h = 1)
        : x(_x), y(_y), w(_w), h(_h)
    {}
    bool isDefaultNorm() const {
        return (x==0 && y==0 && w==1 && h==1);
    }
    friend constexpr inline bool operator==(const RectF &r1, const RectF &r2) noexcept
    {
        return r1.x == r2.x && r1.y == r2.y && r1.w == r2.w && r1.h == r2.h;
    }
    friend constexpr inline bool operator!=(const RectF &r1, const RectF &r2) noexcept
    {
        return !(r1 == r2);
    }
    double x = 0.0;
    double y = 0.0;
    double w = 1.0;
    double h = 1.0;
};

struct Resolution
{
    Resolution(int w=0, int h=0)
        : width(w), height(h)
    {}
    bool isValid() const {
        return (width > 0 && height > 0);
    }
    int width = 0;
    int height = 0;
};

struct ExportInputStatistics
{
    int64_t totalReadBytes = 0;
    float inFps = 0.0f;
    float inKbps = 0.0f;
};

struct ExportOutputStatistics
{
    int64_t totalWriteBytes = 0;
    int64_t outBufferedCount = 0;

    float   outFps = 0.0f;
    float   outKbps = 0.0f;

    std::string currentFilePath;
    bool    currentFileEOF = false;
    int64_t currentWriteBytes = 0;
    std::vector<std::string> exportedFiles;
};

struct PnxFormatInitalizeParameter
{
    // Encryption
    bool usePassword = false;
    std::string password = "";

    // Digital Signing
    bool useDigitalSignature = false;
    std::string certFilePath = "";
    std::string certPassword = "";

    std::string playerFilePath = "";
};

inline int Align16(int value)
{
    return (((value + 15) >> 4) << 4);
}

inline int Align4(int value)
{
    return (((value + 3) >> 2) << 2);
}


}
}
