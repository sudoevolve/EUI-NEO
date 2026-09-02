#include "eui_neo.h"
#include "core/platform/platform.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace app {
namespace {

struct Nv12Frame {
    std::shared_ptr<const std::vector<std::uint8_t>> y;
    std::shared_ptr<const std::vector<std::uint8_t>> uv;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t yStride = 0;
    std::uint32_t uvStride = 0;
    eui::ImageColorSpace colorSpace = eui::ImageColorSpace::BT709;
    eui::ImageColorRange colorRange = eui::ImageColorRange::Limited;
};

eui::ImageColorSpace colorSpaceFor(const AVFrame& frame) {
    switch (frame.colorspace) {
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        return eui::ImageColorSpace::BT601;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        return eui::ImageColorSpace::BT2020;
    case AVCOL_SPC_BT709:
    default:
        return eui::ImageColorSpace::BT709;
    }
}

eui::ImageColorRange colorRangeFor(const AVFrame& frame) {
    return frame.color_range == AVCOL_RANGE_JPEG
        ? eui::ImageColorRange::Full
        : eui::ImageColorRange::Limited;
}

std::string ffmpegError(int code) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, buffer, sizeof(buffer));
    return buffer;
}

bool copyPlane(std::vector<std::uint8_t>& destination,
               std::uint32_t destinationStride,
               const std::uint8_t* source,
               int sourceStride,
               std::uint32_t bytesPerRow,
               std::uint32_t rows) {
    if (source == nullptr || sourceStride == 0 || std::abs(sourceStride) < static_cast<int>(bytesPerRow)) {
        return false;
    }
    destination.resize(static_cast<std::size_t>(destinationStride) * rows);
    for (std::uint32_t row = 0; row < rows; ++row) {
        const auto* sourceRow = source + static_cast<std::ptrdiff_t>(row) * sourceStride;
        auto* destinationRow = destination.data() + static_cast<std::size_t>(row) * destinationStride;
        std::memcpy(destinationRow, sourceRow, bytesPerRow);
    }
    return true;
}

class FfmpegDecoder {
public:
    ~FfmpegDecoder() {
        close();
    }

    bool open(const std::string& source) {
        AVDictionary* options = nullptr;
        av_dict_set(&options, "rw_timeout", "15000000", 0);
        av_dict_set(&options, "timeout", "15000000", 0);
        av_dict_set(&options, "user_agent", "EUI-NEO ffmpeg_video_player", 0);
        AVIOInterruptCB interrupt{&FfmpegDecoder::interruptCallback, this};
        format_ = avformat_alloc_context();
        if (format_ == nullptr) {
            av_dict_free(&options);
            error_ = "无法分配 FFmpeg 输入上下文";
            return false;
        }
        format_->interrupt_callback = interrupt;
        beginNetworkOperation();
        const int openResult = avformat_open_input(&format_, source.c_str(), nullptr, &options);
        endNetworkOperation();
        av_dict_free(&options);
        if (openResult < 0) {
            error_ = networkError("无法打开视频源", openResult);
            return false;
        }
        beginNetworkOperation();
        const int streamInfoResult = avformat_find_stream_info(format_, nullptr);
        endNetworkOperation();
        if (streamInfoResult < 0) {
            error_ = networkError("无法读取视频流信息", streamInfoResult);
            return false;
        }
        streamIndex_ = av_find_best_stream(format_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (streamIndex_ < 0) {
            error_ = "文件中没有视频流";
            return false;
        }

        const AVStream* stream = format_->streams[streamIndex_];
        const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (codec == nullptr) {
            error_ = "找不到对应的视频解码器";
            return false;
        }
        codec_ = avcodec_alloc_context3(codec);
        if (codec_ == nullptr || avcodec_parameters_to_context(codec_, stream->codecpar) < 0 ||
            avcodec_open2(codec_, codec, nullptr) < 0) {
            error_ = "无法初始化视频解码器";
            return false;
        }

        decoded_ = av_frame_alloc();
        nv12_ = av_frame_alloc();
        packet_ = av_packet_alloc();
        if (decoded_ == nullptr || nv12_ == nullptr || packet_ == nullptr) {
            error_ = "无法分配 FFmpeg 解码缓冲区";
            return false;
        }

        double frameRate = av_q2d(stream->avg_frame_rate);
        if (frameRate <= 0.0) {
            frameRate = av_q2d(stream->r_frame_rate);
        }
        frameRate_ = std::clamp(frameRate > 0.0 ? frameRate : 30.0, 1.0, 120.0);
        return true;
    }

    bool next(Nv12Frame& destination) {
        for (;;) {
            const int receiveResult = avcodec_receive_frame(codec_, decoded_);
            if (receiveResult == 0) {
                const bool copied = copyDecodedFrame(destination);
                av_frame_unref(decoded_);
                return copied;
            }
            if (receiveResult == AVERROR_EOF) {
                if (!rewind()) {
                    return false;
                }
                continue;
            }
            if (receiveResult != AVERROR(EAGAIN)) {
                error_ = "读取解码帧失败: " + ffmpegError(receiveResult);
                return false;
            }

            if (draining_) {
                if (!rewind()) {
                    return false;
                }
                continue;
            }

            for (;;) {
                beginNetworkOperation();
                const int readResult = av_read_frame(format_, packet_);
                endNetworkOperation();
                if (readResult < 0) {
                    if (readResult == AVERROR_EXIT) {
                        error_ = "读取视频数据超时";
                        return false;
                    }
                    const int flushResult = avcodec_send_packet(codec_, nullptr);
                    if (flushResult < 0 && flushResult != AVERROR_EOF) {
                        error_ = "刷新解码器失败: " + ffmpegError(flushResult);
                        return false;
                    }
                    draining_ = true;
                    break;
                }
                if (packet_->stream_index != streamIndex_) {
                    av_packet_unref(packet_);
                    continue;
                }
                const int sendResult = avcodec_send_packet(codec_, packet_);
                av_packet_unref(packet_);
                if (sendResult < 0) {
                    error_ = "发送压缩视频帧失败: " + ffmpegError(sendResult);
                    return false;
                }
                break;
            }
        }
    }

    double frameRate() const {
        return frameRate_;
    }

    const std::string& error() const {
        return error_;
    }

private:
    static int interruptCallback(void* opaque) {
        return static_cast<const FfmpegDecoder*>(opaque)->networkTimedOut() ? 1 : 0;
    }

    void beginNetworkOperation() {
        networkDeadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    }

    void endNetworkOperation() {
        networkDeadline_ = {};
    }

    bool networkTimedOut() const {
        return networkDeadline_ != std::chrono::steady_clock::time_point{} &&
               std::chrono::steady_clock::now() >= networkDeadline_;
    }

    std::string networkError(const char* operation, int code) const {
        if (code == AVERROR_EXIT) {
            return std::string(operation) + "超时";
        }
        return std::string(operation) + ": " + ffmpegError(code);
    }

    bool rewind() {
        if (av_seek_frame(format_, streamIndex_, 0, AVSEEK_FLAG_BACKWARD) < 0) {
            error_ = "视频播放结束且无法从开头循环";
            return false;
        }
        avcodec_flush_buffers(codec_);
        draining_ = false;
        return true;
    }

    bool prepareNv12Buffer(int width, int height) {
        if (width == outputWidth_ && height == outputHeight_ &&
            av_frame_make_writable(nv12_) >= 0) {
            return true;
        }
        av_frame_unref(nv12_);
        nv12_->format = AV_PIX_FMT_NV12;
        nv12_->width = width;
        nv12_->height = height;
        if (av_frame_get_buffer(nv12_, 32) < 0) {
            error_ = "无法分配 NV12 转换缓冲区";
            return false;
        }
        outputWidth_ = width;
        outputHeight_ = height;
        return true;
    }

    bool copyDecodedFrame(Nv12Frame& destination) {
        const int width = decoded_->width;
        const int height = decoded_->height;
        if (width <= 0 || height <= 0) {
            error_ = "解码器输出了无效尺寸";
            return false;
        }

        const AVFrame* source = decoded_;
        if (decoded_->format != AV_PIX_FMT_NV12) {
            if (!prepareNv12Buffer(width, height)) {
                return false;
            }
            sws_ = sws_getCachedContext(sws_, width, height,
                                        static_cast<AVPixelFormat>(decoded_->format),
                                        width, height, AV_PIX_FMT_NV12,
                                        SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (sws_ == nullptr ||
                sws_scale(sws_, decoded_->data, decoded_->linesize, 0, height,
                          nv12_->data, nv12_->linesize) <= 0) {
                error_ = "无法将解码帧转换为 NV12";
                return false;
            }
            source = nv12_;
        }

        const std::uint32_t frameWidth = static_cast<std::uint32_t>(width);
        const std::uint32_t frameHeight = static_cast<std::uint32_t>(height);
        const std::uint32_t chromaHeight = (frameHeight + 1u) / 2u;
        const std::uint32_t uvStride = ((frameWidth + 1u) / 2u) * 2u;
        auto y = std::make_shared<std::vector<std::uint8_t>>();
        auto uv = std::make_shared<std::vector<std::uint8_t>>();
        if (!copyPlane(*y, frameWidth, source->data[0], source->linesize[0], frameWidth, frameHeight) ||
            !copyPlane(*uv, uvStride, source->data[1], source->linesize[1], uvStride, chromaHeight)) {
            error_ = "NV12 平面数据不完整";
            return false;
        }
        destination = {y, uv, frameWidth, frameHeight, frameWidth, uvStride,
                       colorSpaceFor(*decoded_), colorRangeFor(*decoded_)};
        return true;
    }

    void close() {
        sws_freeContext(sws_);
        av_packet_free(&packet_);
        av_frame_free(&nv12_);
        av_frame_free(&decoded_);
        avcodec_free_context(&codec_);
        avformat_close_input(&format_);
    }

    AVFormatContext* format_ = nullptr;
    AVCodecContext* codec_ = nullptr;
    AVFrame* decoded_ = nullptr;
    AVFrame* nv12_ = nullptr;
    AVPacket* packet_ = nullptr;
    SwsContext* sws_ = nullptr;
    int streamIndex_ = -1;
    int outputWidth_ = 0;
    int outputHeight_ = 0;
    bool draining_ = false;
    double frameRate_ = 30.0;
    std::chrono::steady_clock::time_point networkDeadline_;
    std::string error_;
};

class VideoPlayer {
public:
    explicit VideoPlayer(std::shared_ptr<eui::ImageStream> stream)
        : stream_(std::move(stream)) {}

    ~VideoPlayer() {
        stop();
    }

    void start(std::string source) {
        if (running_.exchange(true)) {
            return;
        }
        if (worker_.joinable()) {
            worker_.join();
        }
        setStatus("正在打开视频: " + source);
        worker_ = std::thread([this, source = std::move(source)] {
            decodeLoop(source);
        });
    }

    std::string status() const {
        std::lock_guard lock(statusMutex_);
        return status_;
    }

private:
    void stop() {
        running_ = false;
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    void setStatus(std::string value) {
        {
            std::lock_guard lock(statusMutex_);
            status_ = std::move(value);
        }
        core::platform::requestUiUpdate();
    }

    void decodeLoop(const std::string& source) {
        FfmpegDecoder decoder;
        if (!decoder.open(source)) {
            setStatus("打开失败: " + decoder.error());
            running_ = false;
            return;
        }

        setStatus("正在播放 NV12 视频，播放结束后自动循环");
        const auto interval = std::chrono::duration<double>(1.0 / decoder.frameRate());
        auto nextDeadline = std::chrono::steady_clock::now();
        std::uint64_t sequence = 0;
        while (running_) {
            Nv12Frame frame;
            if (!decoder.next(frame)) {
                setStatus("播放失败: " + decoder.error());
                break;
            }
            stream_->submit({frame.y, frame.width, frame.height, frame.yStride,
                             eui::ImagePixelFormat::NV12, sequence++,
                             frame.uv, nullptr, frame.uvStride, 0,
                             frame.colorSpace,
                             frame.colorRange});

            nextDeadline += std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval);
            const auto now = std::chrono::steady_clock::now();
            if (nextDeadline < now) {
                nextDeadline = now;
            }
            std::this_thread::sleep_until(nextDeadline);
        }
        running_ = false;
    }

    std::shared_ptr<eui::ImageStream> stream_;
    std::atomic_bool running_ = false;
    std::thread worker_;
    mutable std::mutex statusMutex_;
    std::string status_ = "等待播放";
};

std::shared_ptr<eui::ImageStream> stream() {
    static auto value = std::make_shared<eui::ImageStream>(2);
    return value;
}

VideoPlayer& player() {
    static VideoPlayer value(stream());
    return value;
}

std::string videoPath() {
#ifdef _WIN32
    char* path = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&path, &length, "EUI_VIDEO_PATH") == 0 && path != nullptr && path[0] != '\0') {
        std::string result(path);
        std::free(path);
        return result;
    }
    std::free(path);
#else
    const char* path = std::getenv("EUI_VIDEO_PATH");
    if (path != nullptr && path[0] != '\0') {
        return path;
    }
#endif
    return "https://samplelib.com/lib/preview/mp4/sample-5s.mp4";
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("FFmpeg Video Player")
        .pageId("ffmpeg_video_player")
        .clearColor({0.025f, 0.030f, 0.045f, 1.0f})
        .windowSize(960, 620)
        .fps(60.0);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    static bool started = false;
    if (!started) {
        player().start(videoPath());
        started = true;
    }

    const float videoWidth = screen.width - 64.0f;
    const float videoHeight = std::min(screen.height - 112.0f, videoWidth * 9.0f / 16.0f);
    ui.column("page")
        .size(screen.width, screen.height)
        .padding(32.0f)
        .gap(12.0f)
        .content([&] {
            ui.text("title").text("FFmpeg -> NV12 -> ImageStream").fontSize(20.0f).build();
            ui.image("video")
                .size(videoWidth, videoHeight)
                .stream(stream())
                .fit(eui::ImageFit::Contain)
                .build();
            ui.text("status").text(player().status()).fontSize(14.0f).build();
        })
        .build();
}

} // namespace app
