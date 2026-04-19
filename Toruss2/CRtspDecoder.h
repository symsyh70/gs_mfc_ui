#pragma once

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
}

#include <opencv2/opencv.hpp>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <string>
#include "VideoFrame.h"
#include <functional>
#include <chrono>

class CRtspDecoder
{
public:
    CRtspDecoder();
    ~CRtspDecoder();

public:
    using FrameArrivedCallback = std::function<void()>;
    using StreamStateCallback = std::function<void(bool connected, bool gaveUp)>;

    void SetFrameArrivedCallback(FrameArrivedCallback cb);
    void SetStreamStateCallback(StreamStateCallback cb);

    bool Open(const std::string& url);
    void Close();

    bool IsOpened() const;
    bool GetLatestFrame(VideoFrame& outFrame);

    void SetLowLatency(bool enable);
    void SetReconnectIntervalMs(int ms);
    void SetReconnectTimeoutMs(int ms);

private:
    FrameArrivedCallback m_frameArrivedCb;
    StreamStateCallback m_streamStateCb;

    bool InitFFmpeg(const std::string& url);
    void ReleaseFFmpeg();

    void DecodeThreadProc();
    bool DecodeOnePacket();
    bool HandleDecodedFrame(AVFrame* pFrame);

    bool OpenCodecContext();
    void UpdateLatestFrame(const VideoFrame& frame);

private:
    static int InterruptCallback(void* ctx);
    int64_t GetNowMs() const;
    void BeginIoTimeoutWatch(int timeoutMs);
    void EndIoTimeoutWatch();

private:
    std::string m_url;

    AVFormatContext* m_fmtCtx = nullptr;
    AVCodecContext* m_codecCtx = nullptr;
    const AVCodec* m_codec = nullptr;
    AVPacket* m_packet = nullptr;
    AVFrame* m_decodedFrame = nullptr;
    AVFrame* m_bgrFrame = nullptr;
    SwsContext* m_swsCtx = nullptr;

    int m_videoStreamIndex = -1;
    uint8_t* m_bgrBuffer = nullptr;
    int m_bgrBufferSize = 0;

    std::thread m_decodeThread;
    std::mutex m_frameMtx;
    std::atomic<bool> m_running = false;
    std::atomic<bool> m_opened = false;
    std::atomic<bool> m_lowLatency = true;

    std::atomic<bool> m_connected = false;
    std::atomic<bool> m_gaveUp = false;

    // FFmpeg block timeout Á¦¾î¿ë
    std::atomic<int64_t> m_ioStartMs{ 0 };
    std::atomic<int>     m_ioTimeoutMs{ 3000 };

    VideoFrame m_latestFrame;
    int64_t m_frameCounter = 0;
    int m_reconnectIntervalMs = 2000;
    int m_reconnectTimeoutMs = 15000;
    std::mutex m_ffmpegMtx;
};