#include "pch.h"
#include "CRtspDecoder.h"

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")

CRtspDecoder::CRtspDecoder()
{
    avformat_network_init();
}

CRtspDecoder::~CRtspDecoder()
{
    Close();
    avformat_network_deinit();
}

void CRtspDecoder::SetLowLatency(bool enable)
{
    m_lowLatency = enable;
}

void CRtspDecoder::SetReconnectIntervalMs(int ms)
{
    if (ms > 0)
        m_reconnectIntervalMs = ms;
}

void CRtspDecoder::SetReconnectTimeoutMs(int ms)
{
    if (ms > 0)
        m_reconnectTimeoutMs = ms;
}

void CRtspDecoder::SetStreamStateCallback(StreamStateCallback cb)
{
    m_streamStateCb = std::move(cb);
}

void CRtspDecoder::SetFrameArrivedCallback(FrameArrivedCallback cb)
{
    m_frameArrivedCb = std::move(cb);
}

bool CRtspDecoder::IsOpened() const
{
    return m_opened;
}

int64_t CRtspDecoder::GetNowMs() const
{
    return av_gettime_relative() / 1000; // ms
}

void CRtspDecoder::BeginIoTimeoutWatch(int timeoutMs)
{
    m_ioTimeoutMs = timeoutMs;
    m_ioStartMs = GetNowMs();
}

void CRtspDecoder::EndIoTimeoutWatch()
{
    m_ioStartMs = 0;
}

int CRtspDecoder::InterruptCallback(void* ctx)
{
    CRtspDecoder* self = static_cast<CRtspDecoder*>(ctx);
    if (!self)
        return 0;

    // Close() 시 즉시 탈출
    if (!self->m_running)
        return 1;

    const int64_t startMs = self->m_ioStartMs.load();
    const int timeoutMs = self->m_ioTimeoutMs.load();

    if (startMs <= 0 || timeoutMs <= 0)
        return 0;

    const int64_t nowMs = self->GetNowMs();
    if ((nowMs - startMs) >= timeoutMs)
        return 1;

    return 0;
}

bool CRtspDecoder::Open(const std::string& url)
{
    Close();

    m_url = url;
    m_gaveUp = false;
    m_connected = false;
    m_opened = false;
    m_running = true;

    CString dbg;
    dbg.Format(L"[CRtspDecoder] Open() url=%S\r\n", url.c_str());
    OutputDebugString(dbg);

    // 최초 1회만 시도
    if (InitFFmpeg(url))
    {
        OutputDebugString(L"[CRtspDecoder] InitFFmpeg OK\r\n");

        m_connected = true;
        m_gaveUp = false;
        m_opened = true;

        if (m_streamStateCb)
            m_streamStateCb(true, false);
    }
    else
    {
        OutputDebugString(L"[CRtspDecoder] InitFFmpeg first try FAIL\r\n");

        m_connected = false;
        m_gaveUp = false;
        m_opened = false;

        if (m_streamStateCb)
            m_streamStateCb(false, false);
    }

    m_decodeThread = std::thread(&CRtspDecoder::DecodeThreadProc, this);
    return m_opened;
}

void CRtspDecoder::Close()
{
    m_running = false;
    m_opened = false;
    m_connected = false;
    m_gaveUp = false;

    // block 중인 FFmpeg 호출 탈출 유도
    BeginIoTimeoutWatch(1);

    if (m_decodeThread.joinable())
        m_decodeThread.join();

    EndIoTimeoutWatch();

    {
        std::lock_guard<std::mutex> lock(m_frameMtx);
        m_latestFrame.Clear();
    }

    ReleaseFFmpeg();
}

bool CRtspDecoder::GetLatestFrame(VideoFrame& outFrame)
{
    std::lock_guard<std::mutex> lock(m_frameMtx);

    if (!m_latestFrame.IsValid())
        return false;

    outFrame.bgr = m_latestFrame.bgr.clone();
    outFrame.width = m_latestFrame.width;
    outFrame.height = m_latestFrame.height;
    outFrame.pts = m_latestFrame.pts;
    outFrame.frameNumber = m_latestFrame.frameNumber;
    return true;
}

bool CRtspDecoder::InitFFmpeg(const std::string& url)
{
    std::lock_guard<std::mutex> lock(m_ffmpegMtx);

    ReleaseFFmpeg();

    AVDictionary* options = nullptr;

    av_dict_set(&options, "rtsp_transport", "tcp", 0);

    if (m_lowLatency)
    {
        av_dict_set(&options, "fflags", "nobuffer", 0);
        av_dict_set(&options, "flags", "low_delay", 0);
        av_dict_set(&options, "max_delay", "0", 0);
        av_dict_set(&options, "buffer_size", "1024000", 0);
    }

    // microseconds
    av_dict_set(&options, "stimeout", "3000000", 0);
    av_dict_set(&options, "rw_timeout", "3000000", 0);

    m_fmtCtx = avformat_alloc_context();
    if (!m_fmtCtx)
    {
        av_dict_free(&options);
        return false;
    }

    m_fmtCtx->interrupt_callback.callback = &CRtspDecoder::InterruptCallback;
    m_fmtCtx->interrupt_callback.opaque = this;

    BeginIoTimeoutWatch(3000);
    int ret = avformat_open_input(&m_fmtCtx, url.c_str(), nullptr, &options);
    EndIoTimeoutWatch();

    av_dict_free(&options);

    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
        av_strerror(ret, errbuf, sizeof(errbuf));

        CStringA msg;
        msg.Format("[CRtspDecoder] avformat_open_input FAIL ret=%d err=%s url=%s\r\n",
            ret, errbuf, url.c_str());
        OutputDebugStringA(msg);

        ReleaseFFmpeg();
        return false;
    }

    if (!m_fmtCtx)
    {
        ReleaseFFmpeg();
        return false;
    }

    BeginIoTimeoutWatch(3000);
    ret = avformat_find_stream_info(m_fmtCtx, nullptr);
    EndIoTimeoutWatch();

    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
        av_strerror(ret, errbuf, sizeof(errbuf));

        CStringA msg;
        msg.Format("[CRtspDecoder] avformat_find_stream_info FAIL ret=%d err=%s\r\n",
            ret, errbuf);
        OutputDebugStringA(msg);

        ReleaseFFmpeg();
        return false;
    }

    m_videoStreamIndex = -1;
    for (unsigned int i = 0; i < m_fmtCtx->nb_streams; ++i)
    {
        if (m_fmtCtx->streams[i] &&
            m_fmtCtx->streams[i]->codecpar &&
            m_fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_videoStreamIndex = static_cast<int>(i);
            break;
        }
    }

    if (m_videoStreamIndex < 0)
    {
        OutputDebugString(L"[CRtspDecoder] video stream not found\r\n");
        ReleaseFFmpeg();
        return false;
    }

    if (!OpenCodecContext())
    {
        OutputDebugString(L"[CRtspDecoder] OpenCodecContext FAIL\r\n");
        ReleaseFFmpeg();
        return false;
    }

    m_packet = av_packet_alloc();
    m_decodedFrame = av_frame_alloc();
    m_bgrFrame = av_frame_alloc();

    if (!m_packet || !m_decodedFrame || !m_bgrFrame)
    {
        OutputDebugString(L"[CRtspDecoder] frame/packet alloc FAIL\r\n");
        ReleaseFFmpeg();
        return false;
    }

    const int width = m_codecCtx->width;
    const int height = m_codecCtx->height;

    if (width <= 0 || height <= 0)
    {
        OutputDebugString(L"[CRtspDecoder] invalid codec size\r\n");
        ReleaseFFmpeg();
        return false;
    }

    m_bgrBufferSize = av_image_get_buffer_size(AV_PIX_FMT_BGR24, width, height, 1);
    if (m_bgrBufferSize <= 0)
    {
        OutputDebugString(L"[CRtspDecoder] av_image_get_buffer_size FAIL\r\n");
        ReleaseFFmpeg();
        return false;
    }

    m_bgrBuffer = static_cast<uint8_t*>(av_malloc(m_bgrBufferSize));
    if (!m_bgrBuffer)
    {
        OutputDebugString(L"[CRtspDecoder] av_malloc FAIL\r\n");
        ReleaseFFmpeg();
        return false;
    }

    if (av_image_fill_arrays(
        m_bgrFrame->data,
        m_bgrFrame->linesize,
        m_bgrBuffer,
        AV_PIX_FMT_BGR24,
        width,
        height,
        1) < 0)
    {
        OutputDebugString(L"[CRtspDecoder] av_image_fill_arrays FAIL\r\n");
        ReleaseFFmpeg();
        return false;
    }

    m_swsCtx = sws_getContext(
        width, height, m_codecCtx->pix_fmt,
        width, height, AV_PIX_FMT_BGR24,
        SWS_FAST_BILINEAR,
        nullptr, nullptr, nullptr);

    if (!m_swsCtx)
    {
        OutputDebugString(L"[CRtspDecoder] sws_getContext FAIL\r\n");
        ReleaseFFmpeg();
        return false;
    }

    return true;
}

bool CRtspDecoder::OpenCodecContext()
{
    AVStream* stream = m_fmtCtx->streams[m_videoStreamIndex];
    AVCodecParameters* codecpar = stream->codecpar;

    m_codec = avcodec_find_decoder(codecpar->codec_id);
    if (!m_codec)
        return false;

    m_codecCtx = avcodec_alloc_context3(m_codec);
    if (!m_codecCtx)
        return false;

    if (avcodec_parameters_to_context(m_codecCtx, codecpar) < 0)
        return false;

    m_codecCtx->thread_count = 2;
    m_codecCtx->thread_type = FF_THREAD_FRAME;

    if (avcodec_open2(m_codecCtx, m_codec, nullptr) < 0)
        return false;

    return true;
}

void CRtspDecoder::ReleaseFFmpeg()
{
    if (m_bgrBuffer)
    {
        av_free(m_bgrBuffer);
        m_bgrBuffer = nullptr;
    }

    if (m_bgrFrame)
        av_frame_free(&m_bgrFrame);

    if (m_decodedFrame)
        av_frame_free(&m_decodedFrame);

    if (m_packet)
        av_packet_free(&m_packet);

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }

    if (m_codecCtx)
        avcodec_free_context(&m_codecCtx);

    if (m_fmtCtx)
        avformat_close_input(&m_fmtCtx);

    m_codec = nullptr;
    m_videoStreamIndex = -1;
    m_bgrBufferSize = 0;
    m_frameCounter = 0;
}

void CRtspDecoder::DecodeThreadProc()
{
    try
    {
        OutputDebugString(L"[CRtspDecoder] DecodeThread start\r\n");

        while (m_running)
        {
            // 연결이 아직 안 되었거나 끊긴 상태면 여기서 재접속
            if (!m_fmtCtx || !m_codecCtx)
            {
                auto reconnectBegin = std::chrono::steady_clock::now();

                while (m_running)
                {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - reconnectBegin).count();

                    if (elapsed >= m_reconnectTimeoutMs)
                    {
                        OutputDebugString(L"[CRtspDecoder] reconnect timeout -> give up\r\n");
                        m_opened = false;
                        m_connected = false;
                        m_gaveUp = true;
                        m_running = false;

                        if (m_streamStateCb)
                            m_streamStateCb(false, true);

                        return;
                    }

                    if (InitFFmpeg(m_url))
                    {
                        OutputDebugString(L"[CRtspDecoder] Reconnect InitFFmpeg OK\r\n");

                        m_connected = true;
                        m_gaveUp = false;
                        m_opened = true;

                        if (m_streamStateCb)
                            m_streamStateCb(true, false);

                        break;
                    }

                    OutputDebugString(L"[CRtspDecoder] Reconnect InitFFmpeg FAIL\r\n");
                    std::this_thread::sleep_for(std::chrono::milliseconds(m_reconnectIntervalMs));
                }

                continue;
            }

            if (DecodeOnePacket())
                continue;

            OutputDebugString(L"[CRtspDecoder] DecodeOnePacket FAIL -> reconnect\r\n");

            m_connected = false;
            m_opened = false;
            ReleaseFFmpeg();

            if (m_streamStateCb)
                m_streamStateCb(false, false);

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        OutputDebugString(L"[CRtspDecoder] DecodeThread end\r\n");
    }
    catch (const cv::Exception& e)
    {
        CStringA msg;
        msg.Format("[CRtspDecoder] cv::Exception: %s\r\n", e.what());
        OutputDebugStringA(msg);
        m_running = false;
        m_opened = false;
    }
    catch (const std::exception& e)
    {
        CStringA msg;
        msg.Format("[CRtspDecoder] std::exception: %s\r\n", e.what());
        OutputDebugStringA(msg);
        m_running = false;
        m_opened = false;
    }
    catch (...)
    {
        OutputDebugString(L"[CRtspDecoder] unknown exception in DecodeThreadProc\r\n");
        m_running = false;
        m_opened = false;
    }
}

bool CRtspDecoder::DecodeOnePacket()
{
    std::lock_guard<std::mutex> lock(m_ffmpegMtx);

    if (!m_fmtCtx || !m_packet || !m_codecCtx || !m_decodedFrame)
        return false;

    BeginIoTimeoutWatch(3000);
    int ret = av_read_frame(m_fmtCtx, m_packet);
    EndIoTimeoutWatch();

    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
        av_strerror(ret, errbuf, sizeof(errbuf));

        CStringA msg;
        msg.Format("[CRtspDecoder] av_read_frame FAIL ret=%d err=%s\r\n", ret, errbuf);
        OutputDebugStringA(msg);

        return false;
    }

    if (m_packet->stream_index != m_videoStreamIndex)
    {
        av_packet_unref(m_packet);
        return true;
    }

    ret = avcodec_send_packet(m_codecCtx, m_packet);
    av_packet_unref(m_packet);

    if (ret < 0)
        return true;

    while (ret >= 0)
    {
        ret = avcodec_receive_frame(m_codecCtx, m_decodedFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;

        if (ret < 0)
            return true;

        if (!HandleDecodedFrame(m_decodedFrame))
            return true;
    }

    return true;
}

bool CRtspDecoder::HandleDecodedFrame(AVFrame* pFrame)
{
    if (!pFrame || !m_swsCtx)
        return false;

    sws_scale(
        m_swsCtx,
        pFrame->data,
        pFrame->linesize,
        0,
        m_codecCtx->height,
        m_bgrFrame->data,
        m_bgrFrame->linesize);

    cv::Mat temp(
        m_codecCtx->height,
        m_codecCtx->width,
        CV_8UC3,
        m_bgrFrame->data[0],
        m_bgrFrame->linesize[0]);

    VideoFrame vf;
    vf.bgr = temp.clone();
    vf.width = temp.cols;
    vf.height = temp.rows;
    vf.pts = pFrame->pts;
    vf.frameNumber = ++m_frameCounter;

    UpdateLatestFrame(vf);

    if (m_frameArrivedCb)
        m_frameArrivedCb();

    return true;
}

void CRtspDecoder::UpdateLatestFrame(const VideoFrame& frame)
{
    std::lock_guard<std::mutex> lock(m_frameMtx);
    m_latestFrame = frame;
}