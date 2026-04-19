#include "pch.h"
#include "VideoView.h"
#include <cmath>
#include "Toruss2Dlg.h"

CVideoView::CVideoView()
{
    m_lastAISubmitTime = std::chrono::steady_clock::now();
    m_lastRenderTime = std::chrono::steady_clock::now();
    // PTZ 커맨드 시각을 epoch 대신 현재 시각으로 초기화 → moving 오판정 방지
    m_lastPTZCmdTime = std::chrono::steady_clock::now();

    StartComposeThread();

    m_decoderColor.SetFrameArrivedCallback([this]()
        {
            MarkComposeDirty();
        });

    m_decoderThermal.SetFrameArrivedCallback([this]()
        {
            MarkComposeDirty();
        });

    m_decoderColor.SetStreamStateCallback([this](bool connected, bool gaveUp)
        {
            if (!connected && gaveUp)
            {
                std::lock_guard<std::mutex> lock(m_mtxColor);
                m_rawColorFrame.release();
                MarkComposeDirty();
            }
        });

    m_decoderThermal.SetStreamStateCallback([this](bool connected, bool gaveUp)
        {
            if (!connected && gaveUp)
            {
                std::lock_guard<std::mutex> lock(m_mtxThermal);
                m_rawThermalFrame.release();
                MarkComposeDirty();
            }
        });
}

CVideoView::~CVideoView()
{
    StopComposeThread();
    StopAIThread();
    CloseStreams();
}

void CVideoView::Initialize(CWnd* pOwner,
    CStatic* pMainView,
    CStatic* pSubColor,
    CStatic* pSubThermal,
    CStatic* pSubMiniMap)
{
    m_pOwner = pOwner;
    m_pMainViewPic = pMainView;
    m_pColorPic = pSubColor;
    m_pThermalPic = pSubThermal;
    m_pMiniMapPic = pSubMiniMap;
}

bool CVideoView::OpenColorStream(const CString& url)
{
    bool expected = false;
    if (!m_colorOpening.compare_exchange_strong(expected, true))
    {
        OutputDebugString(L"[CVideoView] OpenColorStream skip - already opening\r\n");
        return false;
    }

    CStringA urlA(url);

    CString dbg;
    dbg.Format(L"[CVideoView] OpenColorStream = %s\r\n", url.GetString());
    OutputDebugString(dbg);

    m_decoderColor.SetLowLatency(true);
    m_decoderColor.SetReconnectIntervalMs(2000);
    m_decoderColor.SetReconnectTimeoutMs(15000);

    bool ok = m_decoderColor.Open(std::string(urlA.GetString()));
    if (ok)
        StartAIThread();

    OutputDebugString(ok ? L"[CVideoView] OpenColorStream OK\r\n"
        : L"[CVideoView] OpenColorStream FAIL\r\n");

    m_colorOpening = false;
    return ok;
}

bool CVideoView::OpenThermalStream(const CString& url)
{
    CStringA urlA(url);

    CString dbg;
    dbg.Format(L"[CVideoView] OpenThermalStream = %s\r\n", url.GetString());
    OutputDebugString(dbg);

    m_decoderThermal.SetLowLatency(true);
    m_decoderThermal.SetReconnectIntervalMs(2000);
    m_decoderThermal.SetReconnectTimeoutMs(15000); // 15

    bool ok = m_decoderThermal.Open(std::string(urlA.GetString()));

    OutputDebugString(ok ? L"[CVideoView] OpenThermalStream OK\r\n"
        : L"[CVideoView] OpenThermalStream FAIL\r\n");

    return ok;
}

bool CVideoView::LoadMiniMapImage(const CString& path)
{
    bool ok = m_miniMap.LoadMapImage(path);
    if (!ok)
    {
        CString dbg;
        dbg.Format(L"[CVideoView] LoadMiniMapImage FAIL = %s\r\n", path.GetString());
        OutputDebugString(dbg);
        return false;
    }

    MarkComposeDirty();
    return true;
}

void CVideoView::SetMiniMapCameraGeo(double lat, double lon)
{
    m_miniMap.SetCameraGeo(lat, lon);
    MarkComposeDirty();
}

void CVideoView::SetMiniMapPanTilt(double panDeg, double tiltDeg)
{
    m_miniMap.SetCameraPanTilt(panDeg, tiltDeg);
    MarkComposeDirty();
}

void CVideoView::SetMiniMapHFOV(double hfovDeg)
{
    m_miniMap.SetHFOV(hfovDeg);
    MarkComposeDirty();
}

void CVideoView::ZoomMiniMapIn()
{
    m_miniMap.ZoomIn();
    MarkComposeDirty();
}

void CVideoView::ZoomMiniMapOut()
{
    if (!m_pMiniMapPic || !::IsWindow(m_pMiniMapPic->GetSafeHwnd()))
        return;

    CRect rc;
    m_pMiniMapPic->GetClientRect(&rc);

    m_miniMap.ZoomOut(rc.Width(), rc.Height());
    MarkComposeDirty();
}

void CVideoView::CloseStreams()
{
    m_decoderColor.Close();
    m_decoderThermal.Close();

    {
        std::lock_guard<std::mutex> lock1(m_mtxColor);
        m_rawColorFrame.release();
    }

    {
        std::lock_guard<std::mutex> lock2(m_mtxThermal);
        m_rawThermalFrame.release();
    }

    MarkComposeDirty();

}

void CVideoView::RequestRender()
{
    if (!m_pOwner || !::IsWindow(m_pOwner->GetSafeHwnd()))
        return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastRenderTime).count();

    if (elapsed < 33)
        return;

    m_lastRenderTime = now;

    bool expected = false;
    if (m_renderPosted.compare_exchange_strong(expected, true))
    {
        BOOL ok = ::PostMessage(m_pOwner->GetSafeHwnd(), WM_VIDEO_RENDER, 0, 0);
    }
}

void CVideoView::OnRenderMessage()
{
    m_renderPosted = false;
    PaintPreparedFrames();
}

void CVideoView::OnColorFrame(const cv::Mat& frame)
{
    if (frame.empty())
        return;

    {
        std::lock_guard<std::mutex> lock(m_mtxColor);
        m_rawColorFrame = frame.clone();
    }

    // AI  frame ֽ  .
    // backlog  ʰ ֽ ͸ .
    {
        std::lock_guard<std::mutex> lock(m_aiMtx);
        m_aiFrame = frame.clone();
        m_lastAISubmitTime = std::chrono::steady_clock::now();
    }
}

void CVideoView::OnThermalFrame(const cv::Mat& frame)
{
    if (frame.empty())
        return;

    std::lock_guard<std::mutex> lock(m_mtxThermal);
    m_rawThermalFrame = frame.clone();
}

void CVideoView::SetTrackingResults(const std::vector<TrackingResult>& tracking)
{
    {
        std::lock_guard<std::mutex> lock(m_trackMtx);
        m_tracking = tracking;
    }

    MarkComposeDirty();
}

void CVideoView::SetMainFullscreen(bool full)
{
    m_bMainFullscreen = full;
    MarkComposeDirty();
}

void CVideoView::PaintAll()
{
    PaintSourceToControl(m_mainSource, m_pMainViewPic, true);

    if (!m_bMainFullscreen)
    {
        PaintSourceToControl(m_subColorSource, m_pColorPic, false);
        PaintSourceToControl(m_subThermalSource, m_pThermalPic, false);
        PaintSourceToControl(m_subMiniMapSource, m_pMiniMapPic, false);
    }
}

void CVideoView::PaintSourceToControl(VIEW_SOURCE src, CStatic* pCtrl, bool bMain)
{
    if (!pCtrl || !::IsWindow(pCtrl->GetSafeHwnd()))
        return;

    cv::Mat frame;

    CRect rc;
    pCtrl->GetClientRect(&rc);

    const int viewW = rc.Width();
    const int viewH = rc.Height();
    if (viewW <= 0 || viewH <= 0)
        return;

    switch (src)
    {
    case VIEW_SOURCE::COLOR:
    {
        std::lock_guard<std::mutex> lock(m_mtxColor);
        if (m_rawColorFrame.empty())
            return;
        frame = m_rawColorFrame.clone();
        break;
    }
    case VIEW_SOURCE::THERMAL:
    {
        std::lock_guard<std::mutex> lock(m_mtxThermal);
        if (m_rawThermalFrame.empty())
            return;
        frame = m_rawThermalFrame.clone();
        break;
    }
    case VIEW_SOURCE::MINIMAP:
    {
        if (!m_miniMap.Render(viewW, viewH, frame))
            return;
        break;
    }
    default:
        return;
    }

    if (frame.empty())
        return;

    cv::Mat stretched;
    cv::resize(frame, stretched, cv::Size(viewW, viewH), 0, 0, cv::INTER_LINEAR);

    if (bMain && src == VIEW_SOURCE::COLOR)
    {
        std::vector<TrackingResult> tracking;
        {
            std::lock_guard<std::mutex> lock(m_trackMtx);
            tracking = m_tracking;
        }

        const double scaleX = static_cast<double>(viewW) / static_cast<double>(frame.cols);
        const double scaleY = static_cast<double>(viewH) / static_cast<double>(frame.rows);

        for (const auto& t : tracking)
        {
            cv::Rect box;
            box.x = static_cast<int>(std::round(t.box.x * scaleX));
            box.y = static_cast<int>(std::round(t.box.y * scaleY));
            box.width = static_cast<int>(std::round(t.box.width * scaleX));
            box.height = static_cast<int>(std::round(t.box.height * scaleY));

            box &= cv::Rect(0, 0, viewW, viewH);
            if (box.width <= 0 || box.height <= 0)
                continue;

            CStringA labelA;
            labelA.Format("Id: %d  Conf:%.2f  Cls:%d", t.id, t.prob, t.cls);

            cv::putText(
                stretched,
                std::string(labelA),
                cv::Point(box.x, std::max(20, box.y - 5)),
                cv::FONT_HERSHEY_SIMPLEX,
                0.6,
                cv::Scalar(0, 255, 0),
                2
            );

            cv::rectangle(stretched, box, cv::Scalar(0, 255, 0), 2);
        }
    }

    CCvPainter::DrawMatToControl(pCtrl, stretched);
}
void CVideoView::StartAIThread()
{
    if (m_aiRunning)
        return;

    m_aiRunning = true;
    m_aiThread = std::thread(&CVideoView::AIThreadProc, this);
}

void CVideoView::StopAIThread()
{
    m_aiRunning = false;
    if (m_aiThread.joinable())
        m_aiThread.join();
}


void CVideoView::AIThreadProc()
{
    while (m_aiRunning)
    {
        if (m_pMainDlg == nullptr)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        cv::Mat frame;
        {
            std::lock_guard<std::mutex> lock(m_aiMtx);
            if (!m_aiFrame.empty())
            {
                frame = m_aiFrame.clone();
                m_aiFrame.release();
            }
        }

        if (frame.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        UpdatePTZMotionState();

        bool ptzMoving = false;
        float hfov = 0.0f;
        float deltaPan = 0.0f;
        float deltaTilt = 0.0f;
        GetPTZMotionSnapshot(ptzMoving, hfov, deltaPan, deltaTilt);

        if (ptzMoving)
        {
            m_activeTracks = m_pMainDlg->m_DeepAI.get_active_tracks(0);

            m_pMainDlg->m_DeepAI.apply_ptz_motion(0, hfov, deltaPan, deltaTilt); // ← 복원

            m_activeTracks2 = m_pMainDlg->m_DeepAI.get_active_tracks(0);

        }
        std::vector<TrackingResult> tracking =
            m_pMainDlg->m_DeepAI.detect_and_track(frame, 0,0.5);

        SetTrackingResults(tracking);
        UpdateSelectedTrackFromResults(tracking);

        /*     UpdateClickPointJogTracking();*/

        bool clickDriveActive = false;
        {
            std::lock_guard<std::mutex> lock(m_clickDriveMtx);
            clickDriveActive = m_clickDrive.active;
        }

        // ---- track loss 즉시 STOP (이후 추적 루프는 계속 진행) ----
        {
            SelectedTrackState selectedCopy;
            {
                std::lock_guard<std::mutex> lock(m_selectedTrackMtx);
                selectedCopy = m_selectedTrack;
            }

            if (selectedCopy.active)
            {
                // 1) track loss 즉시 STOP
                if (selectedCopy.lossPending)
                {
                    bool shouldStop = false;
                    {
                        std::lock_guard<std::mutex> lock(m_selectedTrackMtx);
                        if (m_selectedTrack.active &&
                            m_selectedTrack.trackId == selectedCopy.trackId &&
                            m_selectedTrack.lossPending &&
                            !m_selectedTrack.lossStopSent)
                        {
                            m_selectedTrack.lossStopSent = true;
                            shouldStop = true;
                        }
                    }

                    if (shouldStop && m_pMainDlg)
                    {
                        SendStopIfNeeded(true);
                        OutputDebugString(L"[Track] loss STOP sent\r\n");
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }

                int frameW = 0;
                int frameH = 0;
                {
                    std::lock_guard<std::mutex> lk(m_mtxColor);
                    if (!m_rawColorFrame.empty())
                    {
                        frameW = m_rawColorFrame.cols;
                        frameH = m_rawColorFrame.rows;
                    }
                }

                if (frameW <= 0 || frameH <= 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }

                const double hfov = m_pMainDlg
                    ? std::max(0.5, m_pMainDlg->GetHFovFromZoom(m_pMainDlg->m_nZoomValue))
                    : 8.0;
                const double vfov = m_pMainDlg
                    ? std::max(0.3, m_pMainDlg->GetVFovFromZoom(m_pMainDlg->m_nZoomValue, frameW, frameH))
                    : 6.0;

                const double bboxCx =
                    selectedCopy.track.box.x + selectedCopy.track.box.width * 0.5;
                const double bboxCy =
                    selectedCopy.track.box.y + selectedCopy.track.box.height * 0.5;

                const CPoint calibCenter =
                    m_pMainDlg->GetCalibratedViewCenter(frameW, frameH);

                const double gapX = bboxCx - calibCenter.x; // FHD 기준
                const double gapY = bboxCy - calibCenter.y;

                const double totalErrorX = (gapX / static_cast<double>(frameW)) * hfov;
                const double totalErrorY = (gapY / static_cast<double>(frameH)) * vfov;

                const double thresholdRatio = 0.25; // bbox 크기의 25% → 필요시 조정
                const double innerThreshX = selectedCopy.track.box.width * thresholdRatio;
                const double innerThreshY = selectedCopy.track.box.height * thresholdRatio;

                const bool insideInnerCrosshair =
                    std::abs(calibCenter.x - bboxCx) <= innerThreshX &&
                    std::abs(calibCenter.y - bboxCy) <= innerThreshY;

                if (insideInnerCrosshair) {
                    m_pMainDlg->m_Command.SendCCB(0x00, 0x00, 0X00, 0x00);
                    m_hasLastCmd = false;
                    wchar_t dbg[128];
                    swprintf_s(dbg, L"[TrackPTZ STOP] errX=%.3f errY=%.3f\r\n",
                        totalErrorX, totalErrorY);
                    OutputDebugString(dbg);
                }
                else {
                    SendTrackErrorAndDrivePTZ(totalErrorX, totalErrorY, hfov, vfov);
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void CVideoView::SetMainDlg(CToruss2Dlg* pMain)
{
    m_pMainDlg = pMain;
}

void CVideoView::MarkComposeDirty()
{
    {
        std::lock_guard<std::mutex> lock(m_composeSrcMtx);
        m_composeDirty = true;
    }
    m_composeCv.notify_one();
}


void CVideoView::StartComposeThread()
{
    if (m_composeRunning)
        return;

    m_composeRunning = true;
    m_composeThread = std::thread(&CVideoView::ComposeThreadProc, this);
}

void CVideoView::StopComposeThread()
{
    m_composeRunning = false;
    m_composeCv.notify_all();

    if (m_composeThread.joinable())
        m_composeThread.join();
}

static cv::Mat StretchToSize(const cv::Mat& src, int w, int h)
{
    if (src.empty() || w <= 0 || h <= 0)
        return cv::Mat();

    cv::Mat out;
    cv::resize(src, out, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);
    return out;
}

void CVideoView::BuildPreparedFrames()
{
    cv::Mat colorFrame;
    cv::Mat thermalFrame;
    cv::Mat miniMapFrame;

    {
        std::lock_guard<std::mutex> lock(m_mtxColor);
        if (!m_rawColorFrame.empty())
            colorFrame = m_rawColorFrame;
    }
    {
        std::lock_guard<std::mutex> lock(m_mtxThermal);
        if (!m_rawThermalFrame.empty())
            thermalFrame = m_rawThermalFrame;
    }

    CRect rcMain, rcColor, rcThermal, rcMini;
    if (!m_pMainViewPic || !m_pColorPic || !m_pThermalPic || !m_pMiniMapPic)
        return;

    m_pMainViewPic->GetClientRect(&rcMain);
    m_pColorPic->GetClientRect(&rcColor);
    m_pThermalPic->GetClientRect(&rcThermal);
    m_pMiniMapPic->GetClientRect(&rcMini);

    m_miniMap.Render(rcMini.Width(), rcMini.Height(), miniMapFrame);

    cv::Mat preparedMain;
    cv::Mat preparedColorSub;
    cv::Mat preparedThermalSub;
    cv::Mat preparedMiniSub;

    auto getSourceFrame = [&](VIEW_SOURCE src) -> cv::Mat
        {
            switch (src)
            {
            case VIEW_SOURCE::COLOR:   return colorFrame;
            case VIEW_SOURCE::THERMAL: return thermalFrame;
            case VIEW_SOURCE::MINIMAP: return miniMapFrame;
            default:                   return cv::Mat();
            }
        };

    cv::Mat srcMain = getSourceFrame(m_mainSource);
    if (!srcMain.empty())
        preparedMain = StretchToSize(srcMain, rcMain.Width(), rcMain.Height());

    if (!m_bMainFullscreen)
    {
        cv::Mat srcColorSub = getSourceFrame(m_subColorSource);
        cv::Mat srcThermalSub = getSourceFrame(m_subThermalSource);
        cv::Mat srcMiniSub = getSourceFrame(m_subMiniMapSource);

        if (!srcColorSub.empty())
            preparedColorSub = StretchToSize(srcColorSub, rcColor.Width(), rcColor.Height());

        if (!srcThermalSub.empty())
            preparedThermalSub = StretchToSize(srcThermalSub, rcThermal.Width(), rcThermal.Height());

        if (!srcMiniSub.empty())
            preparedMiniSub = StretchToSize(srcMiniSub, rcMini.Width(), rcMini.Height());
    }

    // bbox/text  COLOR 
    if (m_mainSource == VIEW_SOURCE::COLOR && !preparedMain.empty() && !colorFrame.empty())
    {
        std::vector<TrackingResult> tracking;
        {
            std::lock_guard<std::mutex> lock(m_trackMtx);
            tracking = m_tracking;
        }

        std::vector<TrackingResult> tracking2;
        {
            std::lock_guard<std::mutex> lock(m_trackMtx);
            tracking2 = m_activeTracks;
        }

        std::vector<TrackingResult> tracking3;
        {
            std::lock_guard<std::mutex> lock(m_trackMtx);
            tracking3 = m_activeTracks2;
        }

        int selectedId = -1;
        bool selectedActive = false;
        {
            std::lock_guard<std::mutex> lock(m_selectedTrackMtx);
            selectedActive = m_selectedTrack.active;
            selectedId = m_selectedTrack.trackId;
        }

        const double scaleX = static_cast<double>(preparedMain.cols) / static_cast<double>(colorFrame.cols);
        const double scaleY = static_cast<double>(preparedMain.rows) / static_cast<double>(colorFrame.rows);

        for (const auto& t : tracking)
        {
            cv::Rect box;
            box.x = static_cast<int>(std::round(t.box.x * scaleX));
            box.y = static_cast<int>(std::round(t.box.y * scaleY));
            box.width = static_cast<int>(std::round(t.box.width * scaleX));
            box.height = static_cast<int>(std::round(t.box.height * scaleY));

            box &= cv::Rect(0, 0, preparedMain.cols, preparedMain.rows);
            if (box.width <= 0 || box.height <= 0)
                continue;

            const bool isSelected = (selectedActive && t.id == selectedId);

            const cv::Scalar color = isSelected
                ? cv::Scalar(0, 0, 255)
                : cv::Scalar(0, 255, 0);

            const int thick = isSelected ? 3 : 2;

            CStringA labelA;
            labelA.Format("ID:%d Conf:%.2f cls:%d", t.id, t.prob, t.cls);

            cv::putText(
                preparedMain,
                std::string(labelA),
                cv::Point(box.x, std::max(20, box.y - 5)),
                cv::FONT_HERSHEY_SIMPLEX,
                0.6,
                color,
                2);

            cv::rectangle(preparedMain, box, color, thick);
        }

        //for (const auto& t : tracking2)
        //{
        //    cv::Rect box;
        //    box.x = static_cast<int>(std::round(t.box.x * scaleX));
        //    box.y = static_cast<int>(std::round(t.box.y * scaleY));
        //    box.width = static_cast<int>(std::round(t.box.width * scaleX));
        //    box.height = static_cast<int>(std::round(t.box.height * scaleY));

        //    box &= cv::Rect(0, 0, preparedMain.cols, preparedMain.rows);
        //    if (box.width <= 0 || box.height <= 0)
        //        continue;

        //    const bool isSelected = (selectedActive && t.id == selectedId);

        //    const cv::Scalar color = cv::Scalar(255, 255, 255);

        //    const int thick = isSelected ? 3 : 2;

        //    CStringA labelA;
        //    labelA.Format("ID:%d Conf:%.2f cls:%d", t.id, t.prob, t.cls);

        //    cv::putText(
        //        preparedMain,
        //        std::string(labelA),
        //        cv::Point(box.x, std::max(20, box.y - 5)),
        //        cv::FONT_HERSHEY_SIMPLEX,
        //        0.6,
        //        color,
        //        2);

        //    cv::rectangle(preparedMain, box, color, thick);
        //}

        //for (const auto& t : tracking3)
        //{
        //    cv::Rect box;
        //    box.x = static_cast<int>(std::round(t.box.x * scaleX));
        //    box.y = static_cast<int>(std::round(t.box.y * scaleY));
        //    box.width = static_cast<int>(std::round(t.box.width * scaleX));
        //    box.height = static_cast<int>(std::round(t.box.height * scaleY));

        //    box &= cv::Rect(0, 0, preparedMain.cols, preparedMain.rows);
        //    if (box.width <= 0 || box.height <= 0)
        //        continue;

        //    const bool isSelected = (selectedActive && t.id == selectedId);

        //    const cv::Scalar color = cv::Scalar(0, 255, 255);

        //    const int thick = isSelected ? 3 : 2;

        //    CStringA labelA;
        //    labelA.Format("ID:%d Conf:%.2f cls:%d", t.id, t.prob, t.cls);

        //    cv::putText(
        //        preparedMain,
        //        std::string(labelA),
        //        cv::Point(box.x, std::max(20, box.y - 5)),
        //        cv::FONT_HERSHEY_SIMPLEX,
        //        0.6,
        //        color,
        //        2);

        //    cv::rectangle(preparedMain, box, color, thick);
        //}

        DrawPTZSpeedOverlay(preparedMain);
        DrawZoomFocusInfo(preparedMain);
        DrawPanTiltDeg(preparedMain);
        DrawCalibratedCrosshair(preparedMain);
    }

    else if (m_mainSource == VIEW_SOURCE::THERMAL && !preparedMain.empty() && !colorFrame.empty())
    {
        DrawPTZSpeedOverlay(preparedMain);
        DrawZoomFocusInfo(preparedMain);
        DrawPanTiltDeg(preparedMain);
    }
    {
        std::lock_guard<std::mutex> lock(m_preparedMtx);
        m_preparedMainFrame = std::move(preparedMain);
        m_preparedColorSubFrame = std::move(preparedColorSub);
        m_preparedThermalSubFrame = std::move(preparedThermalSub);
        m_preparedMiniMapSubFrame = std::move(preparedMiniSub);
    }
}

bool CVideoView::GetLatestColorFrame(cv::Mat& outFrame)
{
    std::lock_guard<std::mutex> lock(m_mtxColor);

    if (m_rawColorFrame.empty())
        return false;

    outFrame = m_rawColorFrame.clone();
    return true;
}

void CVideoView::PaintPreparedFrames()
{
    cv::Mat mainFrame;
    cv::Mat colorSub;
    cv::Mat thermalSub;
    cv::Mat miniSub;

    {
        std::lock_guard<std::mutex> lock(m_preparedMtx);
        mainFrame = m_preparedMainFrame;
        colorSub = m_preparedColorSubFrame;
        thermalSub = m_preparedThermalSubFrame;
        miniSub = m_preparedMiniMapSubFrame;
    }

    CMainVideoView* pMain = dynamic_cast<CMainVideoView*>(m_pMainViewPic);

    if (!mainFrame.empty() && m_pMainViewPic)
    {
        if (pMain)
            pMain->SetVideoActive(true);

        if (pMain)
        {
            DrawMouseDirectionOverlayOnMat(mainFrame, pMain);
        }

        CCvPainter::DrawMatToControl(m_pMainViewPic, mainFrame);
    }
    else
    {
        if (pMain)
            pMain->SetVideoActive(false);
    }

    if (!m_bMainFullscreen)
    {
        if (!colorSub.empty() && m_pColorPic)
            CCvPainter::DrawMatToControl(m_pColorPic, colorSub);

        if (!thermalSub.empty() && m_pThermalPic)
            CCvPainter::DrawMatToControl(m_pThermalPic, thermalSub);

        if (!miniSub.empty() && m_pMiniMapPic)
            CCvPainter::DrawMatToControl(m_pMiniMapPic, miniSub);
    }
}

void CVideoView::SwapMainWith(VIEW_SOURCE src)
{
    if (src == m_mainSource)
        return;

    VIEW_SOURCE oldMain = m_mainSource;
    m_mainSource = src;

    if (m_subColorSource == src)
        m_subColorSource = oldMain;
    else if (m_subThermalSource == src)
        m_subThermalSource = oldMain;
    else if (m_subMiniMapSource == src)
        m_subMiniMapSource = oldMain;

    MarkComposeDirty();
}
void CVideoView::ComposeThreadProc()
{
    while (m_composeRunning)
    {
        {
            std::unique_lock<std::mutex> lock(m_composeSrcMtx);
            m_composeCv.wait(lock, [this]() {
                return !m_composeRunning || m_composeDirty;
                });

            if (!m_composeRunning)
                break;

            m_composeDirty = false;
        }

        VideoFrame vfColor;
        if (m_decoderColor.GetLatestFrame(vfColor))
            OnColorFrame(vfColor.bgr);

        VideoFrame vfThermal;
        if (m_decoderThermal.GetLatestFrame(vfThermal))
            OnThermalFrame(vfThermal.bgr);

        BuildPreparedFrames();
        RequestRender();
    }
}

void CVideoView::DrawMouseDirectionOverlayOnMat(cv::Mat& frame, CMainVideoView* pMain)
{
    if (!pMain || frame.empty())
        return;

    auto dir = pMain->GetHoverDir();
    if (dir == CMainVideoView::HIT_DIR::NONE)
        return;

    CPoint pt = pMain->GetLastMousePt();

    const int size = 64;
    const int margin = 20;

    int cx = frame.cols / 2;
    int cy = frame.rows / 2;

    int drawX = pt.x - size / 2;
    int drawY = pt.y - size / 2;

    drawX = std::max(margin, std::min(drawX, frame.cols - size - margin));
    drawY = std::max(margin, std::min(drawY, frame.rows - size - margin));

    cv::Mat overlay;
    switch (dir)
    {
    case CMainVideoView::HIT_DIR::UP:         overlay = m_imgUp; break;
    case CMainVideoView::HIT_DIR::DOWN:       overlay = m_imgDown; break;
    case CMainVideoView::HIT_DIR::LEFT:       overlay = m_imgLeft; break;
    case CMainVideoView::HIT_DIR::RIGHT:      overlay = m_imgRight; break;
    case CMainVideoView::HIT_DIR::UP_LEFT:    overlay = m_imgUpLeft; break;
    case CMainVideoView::HIT_DIR::UP_RIGHT:   overlay = m_imgUpRight; break;
    case CMainVideoView::HIT_DIR::DOWN_LEFT:  overlay = m_imgDownLeft; break;
    case CMainVideoView::HIT_DIR::DOWN_RIGHT: overlay = m_imgDownRight; break;
    default: return;
    }

    if (overlay.empty())
        return;

    cv::Mat resized;
    cv::resize(overlay, resized, cv::Size(size, size), 0, 0, cv::INTER_LINEAR);
    AlphaBlendImage(frame, resized, drawX, drawY);
}

static void DrawOutlinedText(
    cv::Mat& img,
    const std::string& text,
    cv::Point org,
    double fontScale,
    int thickness,
    const cv::Scalar& textColor,
    const cv::Scalar& outlineColor)
{
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;

    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx == 0 && dy == 0)
                continue;

            cv::putText(img, text, org + cv::Point(dx, dy),
                fontFace, fontScale, outlineColor, thickness + 1, cv::LINE_AA);
        }
    }

    cv::putText(img, text, org,
        fontFace, fontScale, textColor, thickness, cv::LINE_AA);
}

static cv::Mat RotateImageKeepSize(const cv::Mat& src, double angleDeg)
{
    if (src.empty())
        return cv::Mat();

    cv::Point2f center(src.cols * 0.5f, src.rows * 0.5f);
    cv::Mat rot = cv::getRotationMatrix2D(center, angleDeg, 1.0);

    cv::Mat dst;
    cv::warpAffine(
        src, dst, rot, src.size(),
        cv::INTER_LINEAR,
        cv::BORDER_CONSTANT,
        cv::Scalar(0, 0, 0, 0));

    return dst;
}

static void DrawCenteredText(
    cv::Mat& img,
    const CString& text,
    int centerX,
    int topY,
    double fontScale,
    int thickness,
    const cv::Scalar& textColor,
    const cv::Scalar& outlineColor)
{
    CT2A ascii(text);
    std::string str(ascii);

    int baseLine = 0;
    cv::Size ts = cv::getTextSize(
        str, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseLine);

    int x = centerX - ts.width / 2;
    int y = topY + ts.height;

    DrawOutlinedText(img, str, cv::Point(x, y),
        fontScale, thickness, textColor, outlineColor);
}

void CVideoView::DrawPanTiltDeg(cv::Mat& frame)
{
    if (frame.empty())
        return;

    if (m_imgTiltDeg.empty() || m_imgPanDeg.empty())
        return;

    // TODO:    ü
    double tiltDeg = m_pMainDlg->m_fTilt;
    double panDeg = m_pMainDlg->m_fPan;

    const int size = 72;
    const int rightMargin = 12;
    const int bottomMargin = 18;
    const int groupGap = 22;
    const int textGap = 6;

    // Ʒ ׷(PAN) 
    int drawX = frame.cols - size - rightMargin;
    int panY = frame.rows - size - bottomMargin - 18;
    int tiltY = panY - size - 40 - groupGap;

    int centerX = drawX + size / 2;

    const cv::Scalar textColor(206, 254, 1);
    const cv::Scalar textColor2(0, 0, 255);
    const cv::Scalar outlineColor(20, 20, 20);

    // Tilt background
    cv::Mat tiltBg;
    cv::resize(m_imgTiltDeg, tiltBg, cv::Size(size, size), 0, 0, cv::INTER_LINEAR);
    AlphaBlendImage(frame, tiltBg, drawX, tiltY);

    // Tilt arrow
    if (!m_imgTiltDeg_Arrow.empty())
    {
        cv::Mat arrowBase, tiltArrow;
        cv::resize(m_imgTiltDeg_Arrow, arrowBase, cv::Size(size, size), 0, 0, cv::INTER_LINEAR);
        tiltArrow = RotateImageKeepSize(arrowBase, -tiltDeg);
        AlphaBlendImage(frame, tiltArrow, drawX, tiltY);
    }

    // Pan background
    cv::Mat panBg;
    cv::resize(m_imgPanDeg, panBg, cv::Size(size, size), 0, 0, cv::INTER_LINEAR);
    AlphaBlendImage(frame, panBg, drawX, panY);

    CString str;
    str.Format(L"%.4f", tiltDeg);
    DrawCenteredText(frame, str, centerX, tiltY + size + textGap, 0.55, 2, textColor, outlineColor);

    str.Format(L"%.4f", panDeg);
    DrawCenteredText(frame, str, centerX, panY + size + textGap, 0.55, 2, textColor, outlineColor);
}
void CVideoView::DrawCrosshairCalibrationGuide(cv::Mat& frame)
{
    if (frame.empty() || !m_pMainDlg)
        return;

    const int pointCount = m_pMainDlg->GetCrosshairCalibrationPointCount();

    for (int i = 0; i < pointCount; ++i)
    {
        const CPoint pt = m_pMainDlg->GetCrosshairCalibrationPoint(i);
        cv::rectangle(frame,
            cv::Rect(pt.x - 5, pt.y - 5, 10, 10),
            cv::Scalar(40, 40, 40),
            cv::FILLED,
            cv::LINE_AA);
        cv::rectangle(frame,
            cv::Rect(pt.x - 3, pt.y - 3, 6, 6),
            cv::Scalar(255, 160, 0),
            cv::FILLED,
            cv::LINE_AA);

        CString label;
        label.Format(L"%c", (i == 1 || i == 3) ? L'B' : L'A');
        DrawCenteredText(frame, label, pt.x, std::max(12L, static_cast<LONG>(pt.y - 24)), 0.5, 1,
            cv::Scalar(255, 255, 255), cv::Scalar(20, 20, 20));
    }

    if (m_pMainDlg->IsCrosshairCalibrationMode() &&
        m_pMainDlg->HasCrosshairCalibrationLines())
    {
        CPoint s0, e0, s1, e1;
        m_pMainDlg->GetCrosshairCalibrationLine(0, s0, e0);
        m_pMainDlg->GetCrosshairCalibrationLine(1, s1, e1);

        if (s0.x >= 0 && e0.x >= 0)
            cv::line(frame, cv::Point(s0.x, s0.y), cv::Point(e0.x, e0.y), cv::Scalar(255, 160, 0), 1, cv::LINE_AA);
        if (s1.x >= 0 && e1.x >= 0)
            cv::line(frame, cv::Point(s1.x, s1.y), cv::Point(e1.x, e1.y), cv::Scalar(255, 160, 0), 1, cv::LINE_AA);
    }

    if (m_pMainDlg->IsCrosshairCalibrationMode())
    {
        CString guide;
        guide.Format(L"Center Edit: click 4 points (%d/4)", pointCount);
        DrawCenteredText(frame, guide, frame.cols / 2, 24, 0.55, 1,
            cv::Scalar(0, 255, 255), cv::Scalar(20, 20, 20));
    }
}

void CVideoView::DrawCalibratedCrosshair(cv::Mat& frame)
{
    if (frame.empty() || !m_pMainDlg)
        return;

    const CPoint center = m_pMainDlg->GetCalibratedViewCenter(frame.cols, frame.rows);
    const cv::Scalar color = m_pMainDlg->IsCrosshairCalibrationMode()
        ? cv::Scalar(0, 255, 255)
        : cv::Scalar(0, 255, 0);

    const int arm = 20;
    const int gap = 10;
    const int radius = 6;

    cv::line(frame, cv::Point(center.x - arm, center.y), cv::Point(center.x - gap, center.y), color, 1, cv::LINE_AA);
    cv::line(frame, cv::Point(center.x + gap, center.y), cv::Point(center.x + arm, center.y), color, 1, cv::LINE_AA);
    cv::line(frame, cv::Point(center.x, center.y - arm), cv::Point(center.x, center.y - gap), color, 1, cv::LINE_AA);
    cv::line(frame, cv::Point(center.x, center.y + gap), cv::Point(center.x, center.y + arm), color, 1, cv::LINE_AA);
    cv::circle(frame, cv::Point(center.x, center.y), radius, color, 1, cv::LINE_AA);

    DrawCrosshairCalibrationGuide(frame);
}

void CVideoView::LoadOverlayImages(const CString& folderPath)
{
    CStringA pathA(folderPath);

    auto loadPng = [&](const char* name) -> cv::Mat
        {
            std::string full = std::string(pathA.GetString()) + "\\" + name;
            return cv::imread(full, cv::IMREAD_UNCHANGED); // BGRA
        };

    m_imgUp = loadPng("up.png");
    m_imgDown = loadPng("down.png");
    m_imgLeft = loadPng("left.png");
    m_imgRight = loadPng("right.png");
    m_imgUpLeft = loadPng("up-left.png");
    m_imgUpRight = loadPng("up-right.png");
    m_imgDownLeft = loadPng("down-left.png");
    m_imgDownRight = loadPng("down-right.png");

    m_imgTiltDeg_Arrow = loadPng("TiltDeg_Arrow.png");
    m_imgTiltDeg = loadPng("TiltDegBk.png");
    m_imgPanDeg = loadPng("Compas.png");

}

void CVideoView::AlphaBlendImage(cv::Mat& dst, const cv::Mat& src, int x, int y)
{
    if (dst.empty() || src.empty())
        return;

    if (src.channels() != 4 || dst.channels() != 3)
        return;

    for (int sy = 0; sy < src.rows; ++sy)
    {
        int dy = y + sy;
        if (dy < 0 || dy >= dst.rows)
            continue;

        const cv::Vec4b* srcRow = src.ptr<cv::Vec4b>(sy);
        cv::Vec3b* dstRow = dst.ptr<cv::Vec3b>(dy);

        for (int sx = 0; sx < src.cols; ++sx)
        {
            int dx = x + sx;
            if (dx < 0 || dx >= dst.cols)
                continue;

            const cv::Vec4b& s = srcRow[sx];
            cv::Vec3b& d = dstRow[dx];

            double alpha = s[3] / 255.0;
            double inv = 1.0 - alpha;

            d[0] = static_cast<uchar>(d[0] * inv + s[0] * alpha);
            d[1] = static_cast<uchar>(d[1] * inv + s[1] * alpha);
            d[2] = static_cast<uchar>(d[2] * inv + s[2] * alpha);
        }
    }
}

void CVideoView::PanMiniMapByViewDelta(int dxView, int dyView, int viewW, int viewH)
{
    if (viewW <= 0 || viewH <= 0)
        return;

    const double zoom = m_miniMap.GetZoom();
    const int dxImg = (int)std::round(-(double)dxView / zoom);
    const int dyImg = (int)std::round(-(double)dyView / zoom);

    m_miniMap.MoveCenterByPixel(dxImg, dyImg);
    MarkComposeDirty();
}

void CVideoView::ZoomMiniMapAtPoint(int x, int y, int viewW, int viewH, short wheelDelta)
{
    if (viewW <= 0 || viewH <= 0)
        return;

    const double step = (wheelDelta > 0) ? 0.2 : -0.2;
    m_miniMap.ZoomAt(x, y, viewW, viewH, step);
    MarkComposeDirty();
}

bool CVideoView::MiniMapViewPointToLatLon(int x, int y, int viewW, int viewH, double& lat, double& lon)
{
    return m_miniMap.ViewPointToLatLon(x, y, viewW, viewH, lat, lon);
}

bool CVideoView::FindClickedTrackBox(
    const CPoint& clickPt,
    CRect& outBox,
    TrackingResult& outTrack)
{
    if (m_mainSource != VIEW_SOURCE::COLOR)
        return false;

    if (!m_pMainViewPic || !::IsWindow(m_pMainViewPic->GetSafeHwnd()))
        return false;

    cv::Mat colorFrame;
    {
        std::lock_guard<std::mutex> lock(m_mtxColor);
        if (m_rawColorFrame.empty())
            return false;
        colorFrame = m_rawColorFrame.clone();
    }

    if (colorFrame.empty())
        return false;

    CRect rc;
    m_pMainViewPic->GetClientRect(&rc);
    const int viewW = rc.Width();
    const int viewH = rc.Height();

    if (viewW <= 0 || viewH <= 0)
        return false;

    std::vector<TrackingResult> tracking;
    {
        std::lock_guard<std::mutex> lock(m_trackMtx);
        tracking = m_tracking;
    }

    if (tracking.empty())
        return false;

    const double scaleX = static_cast<double>(viewW) / static_cast<double>(colorFrame.cols);
    const double scaleY = static_cast<double>(viewH) / static_cast<double>(colorFrame.rows);

    //  ׷ bbox   hit test
    for (const auto& t : tracking)
    {
        CRect box;
        box.left = static_cast<int>(std::round(t.box.x * scaleX));
        box.top = static_cast<int>(std::round(t.box.y * scaleY));
        box.right = box.left + static_cast<int>(std::round(t.box.width * scaleX));
        box.bottom = box.top + static_cast<int>(std::round(t.box.height * scaleY));

        // ȭ  
        box.IntersectRect(box, rc);

        if (box.Width() <= 0 || box.Height() <= 0)
            continue;

        if (box.PtInRect(clickPt))
        {
            outBox = box;
            outTrack = t;
            return true;
        }
    }

    return false;
}

bool CVideoView::MovePTZToTrackCenter(const CPoint& clickPt)
{
    if (!m_pMainDlg)
        return false;

    CRect hitBox;
    TrackingResult hitTrack;
    if (!FindClickedTrackBox(clickPt, hitBox, hitTrack))
        return false;

    CPoint boxCenter(
        (hitBox.left + hitBox.right) / 2,
        (hitBox.top + hitBox.bottom) / 2);

    return MovePTZToViewPoint(boxCenter);
}

bool CVideoView::MovePTZToViewPoint(const CPoint& targetPt)
{
    if (!m_pMainDlg || !m_pMainViewPic || !::IsWindow(m_pMainViewPic->GetSafeHwnd()))
        return false;

    double errorXDeg = 0.0;
    double errorYDeg = 0.0;
    if (!CalcErrorToViewPoint(targetPt, errorXDeg, errorYDeg))
        return false;

    // bbox 추적은 끊고 클릭 좌표 tracking 시작
    {
        std::lock_guard<std::mutex> lock(m_selectedTrackMtx);
        m_selectedTrack = SelectedTrackState{};
    }

    {
        std::lock_guard<std::mutex> lock(m_clickDriveMtx);
        m_clickDrive = ClickDriveState{};
        m_clickDrive.active = true;
        m_clickDrive.targetPt = targetPt;
    }

    CString dbg;
    dbg.Format(
        L"[ClickJog] start x=%d y=%d errX=%.3f errY=%.3f\r\n",
        targetPt.x, targetPt.y, errorXDeg, errorYDeg);
    OutputDebugString(dbg);

    // 첫 프레임 기다리지 않고 즉시 한 번 반응하게
    UpdateClickPointJogTracking();
    return true;
}

void CVideoView::SendJogDriveByError(
    double errorXDeg,
    double errorYDeg,
    bool& ioCentered,
    bool& ioStopSent,
    double deadZoneX,
    double deadZoneY,
    double resumeX,
    double resumeY)
{
    if (!m_pMainDlg)
        return;

    // ── FOV 비례 deadzone / resume 스케일링 ──────────────────────────────
    // 절대각도 기준 deadzone은 zoom이 높을수록 너무 넓어지므로
    // 현재 HFOV 대비 비율로 스케일: zoom 낮을 땐 더 엄격히, 높을 땐 더 여유있게
    const double curHFov = std::max(0.5, m_pMainDlg->GetHFovFromZoom(m_pMainDlg->m_nZoomValue));
    const double curVFov = std::max(0.3, m_pMainDlg->GetVFovFromZoom(m_pMainDlg->m_nZoomValue, m_rawColorFrame.cols, m_rawColorFrame.rows));

    // FOV의 일정 비율을 deadzone으로 사용 (기준 FOV 39도 기준으로 정규화)
    constexpr double kRefHFov = 39.0;
    constexpr double kRefVFov = 22.0;
    const double fovScaleX = curHFov / kRefHFov;
    const double fovScaleY = curVFov / kRefVFov;

    const double scaledDeadX = deadZoneX * fovScaleX;
    const double scaledDeadY = deadZoneY * fovScaleY;
    const double scaledResumeX = resumeX * fovScaleX;
    const double scaledResumeY = resumeY * fovScaleY;

    const bool inDeadZone =
        (std::abs(errorXDeg) <= scaledDeadX &&
            std::abs(errorYDeg) <= scaledDeadY);

    const bool outsideResume =
        (std::abs(errorXDeg) > scaledResumeX ||
            std::abs(errorYDeg) > scaledResumeY);

    bool needStop = false;
    bool needDrive = false;

    if (inDeadZone)
    {
        if (!ioCentered)
        {
            ioCentered = true;
            ioStopSent = false;
            OutputDebugString(L"[Jog] reached center\r\n");
        }

        if (!ioStopSent)
        {
            needStop = true;
            ioStopSent = true;
        }
    }
    else if (ioCentered)
    {
        if (outsideResume)
        {
            ioCentered = false;
            ioStopSent = false;
            needDrive = true;
            OutputDebugString(L"[Jog] center lost -> resume\r\n");
        }
    }
    else
    {
        needDrive = true;
    }

    if (needStop)
    {
        m_pMainDlg->m_Command.SendCCB(0x00, 0x00, 0x0C, 0x0C);
        return;
    }

    if (!needDrive)
        return;

    // ── 명령 주기: 오차 크기와 무관하게 80ms 고정 ──────────────────────
    // 카메라 관성 지연이 크므로 너무 자주 보내면 오히려 overshooting 악화
    constexpr long long kCmdIntervalMs = 80;

    const auto now = std::chrono::steady_clock::now();
    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_lastPTZCmdTime).count();

    if (elapsedMs < kCmdIntervalMs)
        return;

    m_lastPTZCmdTime = now;

    int panDir = 0;
    int tiltDir = 0;

    if (errorXDeg > 0.0) panDir = 1;
    else if (errorXDeg < 0.0) panDir = -1;

    if (errorYDeg > 0.0) tiltDir = -1;   // 화면 아래 = tilt down
    else if (errorYDeg < 0.0) tiltDir = 1;

    // ── 속도 계산: 실측 기반 quadratic 커브 ──────────────────────────────
    // 실측: panSpd=27일 때 dPan≈2.9도/프레임 (약 33ms 기준)
    // deadzone(HFOV의 ~2%)에 안착하려면 진입 직전 speed ≤ 8 수준이어야 함
    //
    // quadratic 커브 (ratio²): sqrt 커브보다 가까울수록 훨씬 급격히 감속
    //   err=50%HFOV → speed=18 → dPan≈1.9도  (초기 접근 속도)
    //   err=20%HFOV → speed=8  → dPan≈0.9도  (중간)
    //   err=10%HFOV → speed=5  → dPan≈0.5도  (deadzone 직전)
    //   err= 5%HFOV → speed=4  → dPan≈0.4도  (deadzone 내)
    auto calcSpeedByRatio = [](double absErrDeg, double fovDeg,
        int minSpd, int maxSpd) -> int
        {
            if (absErrDeg <= 0.0) return 0;
            // 오차가 FOV의 50%일 때 maxSpd에 도달
            const double ratio = std::min(1.0, absErrDeg / (fovDeg * 0.5));
            // quadratic: 가까울수록 급격히 감속 (sqrt 반대)
            const double curved = ratio * ratio;
            const int spd = minSpd + static_cast<int>(std::round((maxSpd - minSpd) * curved));
            return std::max(0, std::min(63, spd));
        };

    // maxSpd: 실측 기반으로 대폭 축소 (35→18, 28→14)
    const int panSpeed = calcSpeedByRatio(std::abs(errorXDeg), curHFov, 4, 18);
    const int tiltSpeed = calcSpeedByRatio(std::abs(errorYDeg), curVFov, 3, 14);

    BYTE cmd = 0x00;
    if (panDir < 0 && tiltDir > 0)  cmd = 0x0C;
    else if (panDir > 0 && tiltDir > 0)  cmd = 0x0A;
    else if (panDir < 0 && tiltDir < 0)  cmd = 0x14;
    else if (panDir > 0 && tiltDir < 0)  cmd = 0x12;
    else if (panDir < 0)                 cmd = 0x04;
    else if (panDir > 0)                 cmd = 0x02;
    else if (tiltDir > 0)                cmd = 0x08;
    else if (tiltDir < 0)                cmd = 0x10;
    else                                 cmd = 0x00;

    if (cmd == 0x00)
        m_pMainDlg->m_Command.SendCCB(0x00, 0x00, 0x0C, 0x0C);
    else
        m_pMainDlg->m_Command.SendCCB(0x00, cmd, (BYTE)panSpeed, (BYTE)tiltSpeed);

#ifdef _DEBUG
    {
        CString __dbg2;
        __dbg2.Format(
            L"[JogCmd1] errX=%.4f errY=%.4f panDir=%d tiltDir=%d panSpd=%d tiltSpd=%d cmd=0x%02X\r\n",
            errorXDeg, errorYDeg, panDir, tiltDir, panSpeed, tiltSpeed, (int)cmd);
        OutputDebugString(__dbg2);
    }
#endif
}

void CVideoView::SendTrackErrorAndDrivePTZ(double errorX, double errorY, double hFov, double vFov)
{
    constexpr long long kTrackCmdIntervalMs = 80;
    const auto now = std::chrono::steady_clock::now();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastPTZCmdTime).count();

    if (elapsedMs < kTrackCmdIntervalMs)
        return;  // 아직 간격 안 됐으면 skip

    m_lastPTZCmdTime = now;

    // ===== 1. 방향 판단 =====
    const double dirDeadband = 0.05; // deg, 너무 작은 오차 무시
    int panDir = 0;
    if (errorX > dirDeadband)         panDir = 1;
    else if (errorX < -dirDeadband)   panDir = -1;

    int tiltDir = 0;
    if (errorY > dirDeadband)        tiltDir = -1;
    else if (errorY < -dirDeadband)  tiltDir = 1;

    // ===== 2. 커맨드 결정 =====
    BYTE cmd = 0x00;
    if (panDir < 0 && tiltDir > 0)  cmd = 0x0C;
    else if (panDir > 0 && tiltDir > 0)  cmd = 0x0A;
    else if (panDir < 0 && tiltDir < 0)  cmd = 0x14;
    else if (panDir > 0 && tiltDir < 0)  cmd = 0x12;
    else if (panDir < 0)                 cmd = 0x04;
    else if (panDir > 0)                 cmd = 0x02;
    else if (tiltDir > 0)                cmd = 0x08;
    else if (tiltDir < 0)                cmd = 0x10;

    // ===== 4. 속도 계산 (화각 비율 기반) =====
    // errorX/Y는 deg 단위, FOV 대비 비율로 정규화 후 속도 매핑

    // 오차 / 화각 → 0.0 ~ 1.0 정규화
    const double ratioX = std::min(0.3, std::abs(errorX) / hFov);
    const double ratioY = std::min(0.3, std::abs(errorY) / vFov);

    // 속도 범위: 0x01 ~ 0x3F (CCB 스펙 일반적 범위)
    const int SPD_MIN = 0x01;
    const int SPD_MAX = 0x3F;
    const int panSpd = static_cast<int>(SPD_MIN + ratioX * (SPD_MAX - SPD_MIN));
    const int tiltSpd = static_cast<int>(SPD_MIN + ratioY * (SPD_MAX - SPD_MIN));

    // ===== 5. 전송 =====
    m_pMainDlg->m_Command.SendCCB(0x00, cmd,
        static_cast<BYTE>(panSpd),
        static_cast<BYTE>(tiltSpd));

    m_hasLastCmd = true;

    wchar_t dbg[128];
    swprintf_s(dbg, L"[TrackPTZ] cmd=0x%02X panSpd=%d tiltSpd=%d errX=%.3f errY=%.3f\r\n",
        cmd, panSpd, tiltSpd, errorX, errorY);
    OutputDebugString(dbg);
}

void CVideoView::SendTrackErrorToAI(double errorX, double errorY, double hfov)
{
    UNREFERENCED_PARAMETER(errorX);
    UNREFERENCED_PARAMETER(errorY);
    UNREFERENCED_PARAMETER(hfov);

}

bool CVideoView::CalcErrorToViewPoint(const CPoint& targetPt, double& errorX, double& errorY) const
{
    errorX = 0.0;
    errorY = 0.0;

    if (!m_pMainDlg || !m_pMainViewPic || !::IsWindow(m_pMainViewPic->GetSafeHwnd()))
        return false;

    CRect rc;
    m_pMainViewPic->GetClientRect(&rc);

    const int viewW = rc.Width();
    const int viewH = rc.Height();
    if (viewW <= 0 || viewH <= 0)
        return false;

    const CPoint calibratedCenter = m_pMainDlg->GetCalibratedViewCenter(viewW, viewH);

    const double dx = static_cast<double>(targetPt.x - calibratedCenter.x);
    const double dy = static_cast<double>(targetPt.y - calibratedCenter.y);

    // ── FOV는 m_nZoomValue 기준으로 통일 (CalcMoveTime 과 동일 소스) ──
    const double hfovDeg = m_pMainDlg->GetHFovFromZoom(m_pMainDlg->m_nZoomValue);
    const double vfovDeg = m_pMainDlg->GetVFovFromZoom(m_pMainDlg->m_nZoomValue, m_rawColorFrame.cols, m_rawColorFrame.rows);

    // errorX/Y " ȭ鿡   °"
    errorX = (dx / static_cast<double>(viewW)) * hfovDeg;
    errorY = (dy / static_cast<double>(viewH)) * vfovDeg;

#ifdef _DEBUG
    {
        CString __dbg;
        __dbg.Format(
            L"[CalcErr] target=(%d,%d) calibCenter=(%d,%d) dx=%.1f dy=%.1f hfov=%.2f vfov=%.2f errX=%.4f errY=%.4f\r\n",
            targetPt.x, targetPt.y, calibratedCenter.x, calibratedCenter.y,
            dx, dy, hfovDeg, vfovDeg, errorX, errorY);
        OutputDebugString(__dbg);
    }
#endif

    return true;
}

double CVideoView::CalcMoveTimeToViewPoint(const CPoint& targetPt) const
{
    if (!m_pMainDlg || !m_pMainViewPic || !::IsWindow(m_pMainViewPic->GetSafeHwnd()))
        return 0.0;

    CRect rc;
    m_pMainViewPic->GetClientRect(&rc);

    const int viewW = rc.Width();
    const int viewH = rc.Height();
    if (viewW <= 0 || viewH <= 0)
        return 0.0;

    const CPoint calibratedCenter = m_pMainDlg->GetCalibratedViewCenter(viewW, viewH);

    const double dx = static_cast<double>(targetPt.x - calibratedCenter.x);
    const double dy = static_cast<double>(targetPt.y - calibratedCenter.y);

    // ── FOV 소스를 CalcErrorToViewPoint 와 통일 ──
    const double hfovDeg = m_pMainDlg->GetHFovFromZoom(m_pMainDlg->m_nZoomValue);
    double vfovDeg = m_pMainDlg->GetVFovFromZoom(m_pMainDlg->m_nZoomValue, m_rawColorFrame.cols, m_rawColorFrame.rows);
    if (vfovDeg <= 0.0)
        vfovDeg = hfovDeg * static_cast<double>(viewH) / static_cast<double>(viewW);

    const double panErrorDeg = (dx / static_cast<double>(viewW)) * hfovDeg;
    const double tiltErrorDeg = (dy / static_cast<double>(viewH)) * vfovDeg;

    double timePan = 0.0;
    double timeTilt = 0.0;

    if (m_ptzSpeed.panDegPerSec > 0.0)
        timePan = std::abs(panErrorDeg) / m_ptzSpeed.panDegPerSec;

    if (m_ptzSpeed.tiltDegPerSec > 0.0)
        timeTilt = std::abs(tiltErrorDeg) / m_ptzSpeed.tiltDegPerSec;

    // ణ ð
    const double total = std::max(timePan, timeTilt) + 0.15;
    return total;
}

bool CVideoView::StartClickDriveFromBBox(const CPoint& clickPt)
{
    CRect hitBox;
    TrackingResult hitTrack;
    if (!FindClickedTrackBox(clickPt, hitBox, hitTrack))
        return false;

    {
        std::lock_guard<std::mutex> lock(m_clickDriveMtx);
        m_clickDrive = ClickDriveState{};
    }

    std::lock_guard<std::mutex> lock(m_selectedTrackMtx);

    if (m_selectedTrack.active && m_selectedTrack.trackId == hitTrack.id)
    {
        m_centerHoldActive = false;
        m_centerHoldUntil = std::chrono::steady_clock::time_point{};

        m_selectedTrack = SelectedTrackState{};
        m_hasLastCmd = false;
        m_inDeadStableCount = 0;
        m_outResumeStableCount = 0;
        OutputDebugString(L"[Track] selected target cleared\r\n");
        MarkComposeDirty();
        return true;
    }

    m_selectedTrack = SelectedTrackState{};
    m_selectedTrack.active = true;
    m_selectedTrack.trackId = hitTrack.id;
    m_selectedTrack.track = hitTrack;

    const auto now = std::chrono::steady_clock::now();
    m_selectedTrack.lastSeenTime = now;
    m_selectedTrack.lastUpdateTime = now;
    m_selectedTrack.prevUpdateTime = now;

    m_selectedTrack.lastCenterNorm = cv::Point2d(-1.0, -1.0);
    m_selectedTrack.prevCenterNorm = cv::Point2d(-1.0, -1.0);

    m_selectedTrack.centered = false;
    m_selectedTrack.stopSent = false;
    m_selectedTrack.lossPending = false;
    m_selectedTrack.lossStopSent = false;

    m_hasLastCmd = false;
    m_inDeadStableCount = 0;
    m_outResumeStableCount = 0;

    CString dbg;
    dbg.Format(L"[Track] selected id=%d\r\n", hitTrack.id);
    OutputDebugString(dbg);

    MarkComposeDirty();
    return true;
}

void CVideoView::StopClickDrive()
{
    {
        std::lock_guard<std::mutex> lock(m_clickDriveMtx);
        m_clickDrive = ClickDriveState{};
    }

    ClearSelectedTrack();

    if (m_pMainDlg)
        m_pMainDlg->m_Command.SendCCB(0x00, 0x00, 0x0C, 0x0C);
}

void CVideoView::ClearSelectedTrack()
{
    std::lock_guard<std::mutex> lock(m_selectedTrackMtx);
    m_selectedTrack = SelectedTrackState{};
    m_hasLastCmd = false;
}

bool CVideoView::UpdateSelectedTrackFromResults(const std::vector<TrackingResult>& tracking)
{
    std::lock_guard<std::mutex> lock(m_selectedTrackMtx);

    if (!m_selectedTrack.active)
        return false;

    const auto now = std::chrono::steady_clock::now();

    for (const auto& t : tracking)
    {
        if (t.id != m_selectedTrack.trackId)
            continue;

        int frameW = 0;
        int frameH = 0;
        {
            std::lock_guard<std::mutex> lk(m_mtxColor);
            if (!m_rawColorFrame.empty())
            {
                frameW = m_rawColorFrame.cols;
                frameH = m_rawColorFrame.rows;
            }
        }

        // 이전값 보존
        m_selectedTrack.prevCenterNorm = m_selectedTrack.lastCenterNorm;
        m_selectedTrack.prevUpdateTime = m_selectedTrack.lastUpdateTime;

        // 새값 반영
        m_selectedTrack.track = t;
        m_selectedTrack.lastSeenTime = now;
        m_selectedTrack.lastUpdateTime = now;
        m_selectedTrack.lossPending = false;
        m_selectedTrack.lossStopSent = false;

        if (frameW > 0 && frameH > 0)
        {
            const double cxNorm =
                (t.box.x + t.box.width * 0.5) / static_cast<double>(frameW);
            const double cyNorm =
                (t.box.y + t.box.height * 0.5) / static_cast<double>(frameH);

            m_selectedTrack.lastCenterNorm = cv::Point2d(cxNorm, cyNorm);
        }

        return true;
    }

    // 이번 프레임에서 선택 트랙 못 찾음
    m_selectedTrack.lossPending = true;
    return false;
}

void CVideoView::UpdatePTZMotionState()
{
    if (!m_pMainDlg)
        return;

    const double curPan = m_pMainDlg->m_fPan;
    const double curTilt = m_pMainDlg->m_fTilt;
    const double curHFov = m_pMainDlg->m_fHFOV;

    std::lock_guard<std::mutex> lock(m_ptzMotionMtx);

    if (!m_ptzMotion.initialized)
    {
        m_ptzMotion.lastPan = curPan;
        m_ptzMotion.lastTilt = curTilt;
        m_ptzMotion.deltaPan = 0.0;
        m_ptzMotion.deltaTilt = 0.0;
        m_ptzMotion.hfov = curHFov;
        m_ptzMotion.moving = false;
        m_ptzMotion.initialized = true;
        return;
    }

    double dPan = curPan - m_ptzMotion.lastPan;
    double dTilt = curTilt - m_ptzMotion.lastTilt;

    while (dPan > 180.0)  dPan -= 360.0;
    while (dPan < -180.0) dPan += 360.0;

    m_ptzMotion.deltaPan = dPan;
    m_ptzMotion.deltaTilt = dTilt;
    m_ptzMotion.hfov = curHFov;

    const bool telemetryMoved =
        (std::abs(dPan) > 0.01) ||
        (std::abs(dTilt) > 0.01);

    const auto now = std::chrono::steady_clock::now();
    const auto recentCmdMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastPTZCmdTime).count();

    // recentCmdMs >= 0 조건은 항상 참이므로 제거 → < 250 만 체크
    m_ptzMotion.moving = telemetryMoved || (recentCmdMs < 250);

    m_ptzMotion.lastPan = curPan;
    m_ptzMotion.lastTilt = curTilt;
}

bool CVideoView::GetPTZMotionSnapshot(bool& moving, float& hfov, float& deltaPan, float& deltaTilt)
{
    std::lock_guard<std::mutex> lock(m_ptzMotionMtx);

    moving = m_ptzMotion.moving;
    hfov = m_ptzMotion.hfov;
    deltaPan = m_ptzMotion.deltaPan;
    deltaTilt = m_ptzMotion.deltaTilt;
    return m_ptzMotion.initialized;
}


void CVideoView::DrawPTZSpeedOverlay(cv::Mat& frame)
{
    if (frame.empty() || !m_pMainDlg)
        return;

    if (!m_pMainDlg->IsManualPTZSpeedVisible())
        return;

    const int percent = m_pMainDlg->GetManualPTZSpeedPercent();

    CString str;
    str.Format(L"Speed: %d%%", percent);

    const double fontScale = 0.8;
    const int thickness = 2;
    int baseLine = 0;

    cv::Size ts = cv::getTextSize(
        std::string(CT2A(str)),
        cv::FONT_HERSHEY_SIMPLEX,
        fontScale,
        thickness,
        &baseLine);

    const int margin = 20;
    const int x = frame.cols - ts.width - margin - 50;
    const int y = margin + ts.height;

    cv::putText(
        frame,
        std::string(CT2A(str)),
        cv::Point(x, y),
        cv::FONT_HERSHEY_SIMPLEX,
        fontScale,
        cv::Scalar(206, 254, 1),
        thickness,
        cv::LINE_AA);
}
void CVideoView::SetMiniMapGeoBounds(double latTop, double latBottom, double lonLeft, double lonRight)
{
    m_miniMap.SetGeoBounds(latTop, latBottom, lonLeft, lonRight);
    MarkComposeDirty();
}

void CVideoView::SetMiniMapRangeMode(bool enable)
{
    m_miniMap.SetRangeMode(enable);
    MarkComposeDirty();
}

bool CVideoView::IsMiniMapRangeMode() const
{
    return m_miniMap.IsRangeMode();
}

bool CVideoView::BeginMiniMapMeasure(int x, int y, int viewW, int viewH)
{
    bool ok = m_miniMap.BeginRangeMeasure(x, y, viewW, viewH);
    if (ok) MarkComposeDirty();
    return ok;
}

bool CVideoView::UpdateMiniMapMeasure(int x, int y, int viewW, int viewH)
{
    bool ok = m_miniMap.UpdateRangeMeasure(x, y, viewW, viewH);
    if (ok) MarkComposeDirty();
    return ok;
}

void CVideoView::EndMiniMapMeasure()
{
    m_miniMap.EndRangeMeasure();
    MarkComposeDirty();
}

void CVideoView::SetMiniMapHoverPoint(int x, int y, int viewW, int viewH)
{
    m_miniMap.SetHoverPoint(x, y, viewW, viewH);
    MarkComposeDirty();
}

CString CVideoView::GetMiniMapHoverText() const
{
    return m_miniMap.GetHoverText();
}

bool CVideoView::IsMiniMapWindow(HWND hWnd) const
{
    if (!hWnd) return false;

    if (m_pMiniMapPic && hWnd == m_pMiniMapPic->GetSafeHwnd() && IsMiniMapOnMiniSub())
        return true;

    if (m_pColorPic && hWnd == m_pColorPic->GetSafeHwnd() && IsMiniMapOnColorSub())
        return true;

    if (m_pThermalPic && hWnd == m_pThermalPic->GetSafeHwnd() && IsMiniMapOnThermalSub())
        return true;

    if (m_pMainViewPic && hWnd == m_pMainViewPic->GetSafeHwnd() && IsMiniMapMain())
        return true;

    return false;
}

CStatic* CVideoView::GetMiniMapControlFromHwnd(HWND hWnd) const
{
    if (!hWnd) return nullptr;

    if (m_pMiniMapPic && hWnd == m_pMiniMapPic->GetSafeHwnd() && IsMiniMapOnMiniSub())
        return m_pMiniMapPic;

    if (m_pColorPic && hWnd == m_pColorPic->GetSafeHwnd() && IsMiniMapOnColorSub())
        return m_pColorPic;

    if (m_pThermalPic && hWnd == m_pThermalPic->GetSafeHwnd() && IsMiniMapOnThermalSub())
        return m_pThermalPic;

    if (m_pMainViewPic && hWnd == m_pMainViewPic->GetSafeHwnd() && IsMiniMapMain())
        return m_pMainViewPic;

    return nullptr;
}

bool CVideoView::CalcPanTiltFromMiniMapLatLon(double lat, double lon, double& outPanDeg, double& outTiltDeg) const
{
    if (!m_pMainDlg)
        return false;

    // 장비 현재 위치 (CToruss2Dlg가 보관)
    const double lat1 = m_pMainDlg->m_fUnitLat;
    const double lon1 = m_pMainDlg->m_fUnitLon;
    const double lat2 = lat;
    const double lon2 = lon;

    constexpr double kDeg2Rad = 3.14159265358979323846 / 180.0;
    constexpr double kRad2Deg = 180.0 / 3.14159265358979323846;

    const double phi1 = lat1 * kDeg2Rad;
    const double phi2 = lat2 * kDeg2Rad;
    const double dLon = (lon2 - lon1) * kDeg2Rad;

    const double y = std::sin(dLon) * std::cos(phi2);
    const double x = std::cos(phi1) * std::sin(phi2) -
        std::sin(phi1) * std::cos(phi2) * std::cos(dLon);

    // atan2 반환값은 이미 (-π, π] → (-180, 180] 범위이므로 추가 정규화 불필요
    double bearing = std::atan2(y, x) * kRad2Deg;

    outPanDeg = bearing;
    outTiltDeg = m_pMainDlg->m_fTilt;  // 고도 정보 없으므로 현재 tilt 유지
    return true;
}

bool CVideoView::MovePTZToMiniMapPoint(int x, int y, int viewW, int viewH)
{
    if (!m_pMainDlg)
        return false;

    double lat = 0.0, lon = 0.0;
    if (!MiniMapViewPointToLatLon(x, y, viewW, viewH, lat, lon))
        return false;

    double targetPan = 0.0, targetTilt = 0.0;
    if (!CalcPanTiltFromMiniMapLatLon(lat, lon, targetPan, targetTilt))
        return false;

    return m_pMainDlg->MoveCameraToPanTilt(
        targetPan,
        targetTilt,
        m_pMainDlg->GetManualPTZSpeed(),
        m_pMainDlg->GetManualPTZSpeed());
}

void CVideoView::DrawZoomFocusInfo(cv::Mat& frame)
{
    if (frame.empty() || m_pMainDlg == nullptr)
        return;

    const int rightMargin = 14;
    const int topMargin = 14;
    const int lineGap = 24;

    const cv::Scalar textColor(206, 254, 1);
    const cv::Scalar outlineColor(20, 20, 20);

    CString s1 = m_pMainDlg->GetMainViewZoomText();
    CString s2 = m_pMainDlg->GetMainViewFocusText();

    std::vector<CString> lines = { s1, s2 };

    int y = topMargin;
    for (const auto& line : lines)
    {
        CStringA a(line);
        std::string str(a);

        int baseLine = 0;
        cv::Size ts = cv::getTextSize(
            str,
            cv::FONT_HERSHEY_SIMPLEX,
            0.65,
            2,
            &baseLine);

        int x = frame.cols - rightMargin - ts.width;

        DrawOutlinedText(
            frame,
            str,
            cv::Point(x, y + ts.height),
            0.65,
            2,
            textColor,
            outlineColor);

        y += lineGap;
    }
}

void CVideoView::UpdateClickPointJogTracking()
{
    if (!m_pMainDlg)
        return;

    // ── targetPt 먼저 읽기 (CalcError는 락 밖에서 수행) ──────────────
    CPoint targetPt;
    {
        std::lock_guard<std::mutex> lock(m_clickDriveMtx);
        if (!m_clickDrive.active)
            return;
        targetPt = m_clickDrive.targetPt;
    }

    double errorXDeg = 0.0;
    double errorYDeg = 0.0;
    if (!CalcErrorToViewPoint(targetPt, errorXDeg, errorYDeg))
        return;

    // ── centered / stopSent 를 락 안에서 읽고 → 수정 → 저장 (TOCTOU 수정) ──
    {
        std::lock_guard<std::mutex> lock(m_clickDriveMtx);
        if (!m_clickDrive.active)   // 에러 계산 사이에 다른 스레드가 끊었을 수 있음
            return;

        // 클릭 이동: bbox tracking보다 약간 여유 있는 deadzone (덜 진동)
        // 기준 FOV(39도) 대비 약 3.3% / 6.4% 비율
        constexpr double kClickDeadX = 1.30;
        constexpr double kClickDeadY = 1.00;
        constexpr double kClickResX = 2.00;
        constexpr double kClickResY = 1.60;

        SendJogDriveByError(
            errorXDeg,
            errorYDeg,
            m_clickDrive.centered,
            m_clickDrive.stopSent,
            kClickDeadX, kClickDeadY,
            kClickResX, kClickResY);

        // done 판정: FOV 스케일 적용 (SendJogDriveByError 내부와 동일 공식)
        const double curHFov = std::max(0.5, m_pMainDlg->GetHFovFromZoom(m_pMainDlg->m_nZoomValue));
        const double curVFov = std::max(0.3, m_pMainDlg->GetVFovFromZoom(m_pMainDlg->m_nZoomValue, m_rawColorFrame.cols, m_rawColorFrame.rows));
        const double scaledDX = kClickDeadX * (curHFov / 39.0);
        const double scaledDY = kClickDeadY * (curVFov / 22.0);

        const bool done =
            (std::abs(errorXDeg) <= scaledDX &&
                std::abs(errorYDeg) <= scaledDY &&
                m_clickDrive.stopSent);

        if (done)
        {
            m_clickDrive = ClickDriveState{};
            OutputDebugString(L"[ClickJog] target reached\r\n");
        }
    }
}

void CVideoView::SendStopIfNeeded(bool force /*= false*/)
{
    if (!m_pMainDlg)
        return;

    const auto now = std::chrono::steady_clock::now();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastStopCmdTime).count();

    if (force || elapsedMs >= 100)
    {
        m_pMainDlg->m_Command.SendCCB(0x00, 0x00, 0x0C, 0x0C);
        m_lastStopCmdTime = now;
        OutputDebugString(L"[Track] STOP resent\r\n");
    }
}