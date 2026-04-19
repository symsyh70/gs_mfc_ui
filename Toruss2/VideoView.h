// VideoView.h

#pragma once
#include <mutex>
#include <atomic>
#include <vector>
#include <condition_variable>
#include <chrono>
#include "CRtspDecoder.h"
#include "CvPainter.h"
#include "MOTWrapper.h"
#include "CMainView.h"
#include "MiniMap.h"

#define WM_VIDEO_RENDER (WM_APP + 400)

class CToruss2Dlg;

class CVideoView
{
public:
    enum class VIEW_SOURCE
    {
        COLOR,
        THERMAL,
        MINIMAP
    };

    struct PTZSpeed
    {
        double panDegPerSec = 60.0;
        double tiltDegPerSec = 40.0;
    };

    struct ClickDriveState
    {
        bool active = false;
        CPoint targetPt = CPoint(0, 0);

        bool centered = false;
        bool stopSent = false;

        double durationSec = 0.0;
        std::chrono::steady_clock::time_point endTime;
    };

    struct SelectedTrackState
    {
        bool active = false;
        int trackId = -1;
        TrackingResult track{};
        std::chrono::steady_clock::time_point lastSeenTime;

        // 현재 프레임 갱신 시각/중심
        std::chrono::steady_clock::time_point lastUpdateTime;
        cv::Point2d lastCenterNorm = cv::Point2d(-1.0, -1.0);

        // 이전 프레임 중심/시각 (feedforward용)
        std::chrono::steady_clock::time_point prevUpdateTime;
        cv::Point2d prevCenterNorm = cv::Point2d(-1.0, -1.0);

        bool centered = false;
        bool stopSent = false;

        bool lossPending = false;
        bool lossStopSent = false;
    };
private:
    void UpdateClickPointJogTracking();
    void SendJogDriveByError(
        double errorXDeg,
        double errorYDeg,
        bool& ioCentered,
        bool& ioStopSent,
        double deadZoneX,
        double deadZoneY,
        double resumeX,
        double resumeY);

public:
    CVideoView();
    ~CVideoView();

    void SendStopIfNeeded(bool force /*= false*/);

    int m_renderTick = 0;
    void Initialize(CWnd* pOwner,
        CStatic* pMainView,
        CStatic* pSubColor,
        CStatic* pSubThermal,
        CStatic* pSubMiniMap);

    std::atomic<bool> m_colorOpening = false;
    std::atomic<bool> m_thermalOpening = false;

    VIEW_SOURCE GetMainSource() const { return m_mainSource; }

    std::chrono::steady_clock::time_point m_lastRenderTime;
    std::chrono::steady_clock::time_point m_lastStopCmdTime{};

    bool OpenColorStream(const CString& url);
    bool OpenThermalStream(const CString& url);
    void CloseStreams();

    void OnRenderMessage();
    void SetTrackingResults(const std::vector<TrackingResult>& tracking);

    void SwapMainWith(VIEW_SOURCE src);
    void SetMainFullscreen(bool full);

    bool LoadMiniMapImage(const CString& path);
    void SetMiniMapCameraGeo(double lat, double lon);
    void SetMiniMapPanTilt(double panDeg, double tiltDeg);
    void SetMiniMapHFOV(double hfovDeg);
    void ZoomMiniMapIn();
    void ZoomMiniMapOut();

    void PanMiniMapByViewDelta(int dxView, int dyView, int viewW, int viewH);
    void ZoomMiniMapAtPoint(int x, int y, int viewW, int viewH, short wheelDelta);
    bool MiniMapViewPointToLatLon(int x, int y, int viewW, int viewH, double& lat, double& lon);

    void SetMainDlg(CToruss2Dlg* pMain);
    void StartComposeThread();
    void StopComposeThread();
    void ComposeThreadProc();
    void MarkComposeDirty();

    void BuildPreparedFrames();
    void PaintPreparedFrames();
    bool GetLatestColorFrame(cv::Mat& outFrame);

    CMainVideoView* pMain = nullptr;

    private:
        std::chrono::steady_clock::time_point m_centerHoldUntil{};
        bool m_centerHoldActive = false;

public:
    bool FindClickedTrackBox(
        const CPoint& clickPt,
        CRect& outBox,
        TrackingResult& outTrack);

    bool MovePTZToTrackCenter(const CPoint& clickPt);
    bool MovePTZToViewPoint(const CPoint& targetPt);

    bool StartClickDriveFromBBox(const CPoint& clickPt);
    double CalcMoveTimeToViewPoint(const CPoint& targetPt) const;
    bool CalcErrorToViewPoint(const CPoint& targetPt, double& errorX, double& errorY) const;
    void StopClickDrive();

public:
    cv::Mat m_imgUp;
    cv::Mat m_imgDown;
    cv::Mat m_imgLeft;
    cv::Mat m_imgRight;
    cv::Mat m_imgUpLeft;
    cv::Mat m_imgUpRight;
    cv::Mat m_imgDownLeft;
    cv::Mat m_imgDownRight;
    cv::Mat m_imgTiltDeg;
    cv::Mat m_imgTiltDeg_Arrow;
    cv::Mat m_imgPanDeg;

    void LoadOverlayImages(const CString& folderPath);
    void DrawMouseDirectionOverlayOnMat(cv::Mat& frame, CMainVideoView* pMain);
    void DrawPanTiltDeg(cv::Mat& frame);
    void DrawCalibratedCrosshair(cv::Mat& frame);
    void DrawCrosshairCalibrationGuide(cv::Mat& frame);
    static void AlphaBlendImage(cv::Mat& dst, const cv::Mat& src, int x, int y);

    bool IsMiniMapMain() const { return m_mainSource == VIEW_SOURCE::MINIMAP; }
    bool IsMiniMapOnColorSub() const { return m_subColorSource == VIEW_SOURCE::MINIMAP; }
    bool IsMiniMapOnThermalSub() const { return m_subThermalSource == VIEW_SOURCE::MINIMAP; }
    bool IsMiniMapOnMiniSub() const { return m_subMiniMapSource == VIEW_SOURCE::MINIMAP; }

private:
    std::thread m_composeThread;
    std::atomic<bool> m_composeRunning = false;

    std::mutex m_composeSrcMtx;
    std::condition_variable m_composeCv;
    bool m_composeDirty = false;

    std::mutex m_preparedMtx;
    cv::Mat m_preparedMainFrame;
    cv::Mat m_preparedColorSubFrame;
    cv::Mat m_preparedThermalSubFrame;
    cv::Mat m_preparedMiniMapSubFrame;

    void RequestRender();
    void OnColorFrame(const cv::Mat& frame);
    void OnThermalFrame(const cv::Mat& frame);
    void PaintAll();
    void PaintSourceToControl(VIEW_SOURCE src, CStatic* pCtrl, bool bMain);

    void StartAIThread();
    void StopAIThread();
    void AIThreadProc();

    // VideoView.h

private:
    struct PTZMotionState
    {
        bool moving = false;
        double lastPan = 0.0;
        double lastTilt = 0.0;
        float deltaPan = 0.0f;
        float deltaTilt = 0.0f;
        float hfov = 0.0f;
        bool initialized = false;
    };

    mutable std::mutex m_ptzMotionMtx;
    PTZMotionState m_ptzMotion;

    void UpdatePTZMotionState();
    bool GetPTZMotionSnapshot(bool& moving, float& hfov, float& deltaPan, float& deltaTilt);

    void SendTrackErrorToAI(double errorX, double errorY, double hfov);
    bool CalcErrorToTrack(const TrackingResult& trk, double& errorX, double& errorY) const;
    bool UpdateSelectedTrackFromResults(const std::vector<TrackingResult>& tracking);
    void ClearSelectedTrack();
    void SendTrackErrorAndDrivePTZ(double errorX, double errorY, double hFov, double vFov);


    BOOL test = FALSE;

private:

    double m_lastCmdPan = 0.0;
    double m_lastCmdTilt = 0.0;
    bool   m_hasLastCmd = false;

    std::chrono::steady_clock::time_point m_lastPTZCmdTime;
    CToruss2Dlg* m_pMainDlg = nullptr;
    CWnd* m_pOwner = nullptr;
    CStatic* m_pMainViewPic = nullptr;
    CStatic* m_pColorPic = nullptr;
    CStatic* m_pThermalPic = nullptr;
    CStatic* m_pMiniMapPic = nullptr;

    CRtspDecoder m_decoderColor;
    CRtspDecoder m_decoderThermal;

    mutable std::mutex m_mtxColor;
    cv::Mat m_rawColorFrame;

    std::mutex m_mtxThermal;
    cv::Mat m_rawThermalFrame;

    CMiniMap m_miniMap;

    std::mutex m_trackMtx;
    std::vector<TrackingResult> m_tracking;

    std::vector<TrackingResult> m_activeTracks;
    std::vector<TrackingResult> m_activeTracks2;
    std::mutex m_activeTrackMtx;

private:
    int m_inDeadStableCount = 0;
    int m_outResumeStableCount = 0;
    std::chrono::steady_clock::time_point m_retrackGuardUntil;

    std::atomic<bool> m_renderPosted = false;

    VIEW_SOURCE m_mainSource = VIEW_SOURCE::COLOR;
    VIEW_SOURCE m_subColorSource = VIEW_SOURCE::COLOR;
    VIEW_SOURCE m_subThermalSource = VIEW_SOURCE::THERMAL;
    VIEW_SOURCE m_subMiniMapSource = VIEW_SOURCE::MINIMAP;

    void DrawPTZSpeedOverlay(cv::Mat& frame);

    bool m_bMainFullscreen = false;

    std::thread m_aiThread;
    std::mutex m_aiMtx;
    cv::Mat m_aiFrame;
    std::atomic<bool> m_aiRunning = false;
    std::chrono::steady_clock::time_point m_lastAISubmitTime;

    mutable std::mutex m_clickDriveMtx;
    ClickDriveState m_clickDrive;
    PTZSpeed m_ptzSpeed;

    std::mutex m_selectedTrackMtx;
    SelectedTrackState m_selectedTrack;
private:
    bool CalcPanTiltFromMiniMapLatLon(double lat, double lon, double& outPanDeg, double& outTiltDeg) const;
    void DrawZoomFocusInfo(cv::Mat& frame);

public:
    void SetMiniMapGeoBounds(double latTop, double latBottom, double lonLeft, double lonRight);
    void SetMiniMapRangeMode(bool enable);
    bool IsMiniMapRangeMode() const;

    bool BeginMiniMapMeasure(int x, int y, int viewW, int viewH);
    bool UpdateMiniMapMeasure(int x, int y, int viewW, int viewH);
    void EndMiniMapMeasure();

    void SetMiniMapHoverPoint(int x, int y, int viewW, int viewH);
    CString GetMiniMapHoverText() const;

    bool MovePTZToMiniMapPoint(int x, int y, int viewW, int viewH);

    bool IsMiniMapWindow(HWND hWnd) const;
    CStatic* GetMiniMapControlFromHwnd(HWND hWnd) const;
};