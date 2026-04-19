#pragma once
#include "afxdialogex.h"
#include <opencv2/opencv.hpp>
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include "CControl_Command.h"
#include "VideoView.h"

class CControl_Panorama : public CDialogEx
{
public:
    CControl_Panorama(CWnd* pParent = nullptr);
    virtual ~CControl_Panorama();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIALOG_PANORAMA };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()

public:
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnBnClickedButtonPanomake();

    void SetFrame(const cv::Mat& frame);
    void ClearFrame();

    void SetCommManager(CControl_Command* pComm);
    void SetVideoView(CVideoView* pVideoView);

    void SetHFOV(double hfovDeg) { m_hfovDeg = hfovDeg; }
    void SetVFOV(double vfovDeg) { m_vfovDeg = vfovDeg; }
    void SetOverlap(double overlap) { m_overlapRatio = overlap; }

    void SetCurrentView(double pan, double tilt, double hfov, double vfov);

private:
    double m_curPan = 0.0;
    double m_curTilt = 0.0;
    double m_curHFOV = 39.79;
    double m_curVFOV = 22.0;

private:
    struct PanoramaShot
    {
        double panDeg = 0.0;   // 0 ~ 360
        double tiltDeg = 0.0;
        cv::Mat frame;
    };

private:
    static double Normalize180(double deg);
    static double Normalize360(double deg);
    static double Pan180ToPan360(double pan180);
    static double Clamp(double v, double lo, double hi);

    void RunPanoramaCapture360();
    bool CaptureBestFrame(cv::Mat& outFrame, int sampleCount = 5, int intervalMs = 80);
    double CalcSharpness(const cv::Mat& img);

    cv::Mat CylindricalWarp(const cv::Mat& src, double hfovDeg);
    void BlendShot360(
        const cv::Mat& warped,
        double panDeg,
        double hfovDeg,
        cv::Mat& accum,
        cv::Mat& weight);

    cv::Mat BuildPanorama360(const std::vector<PanoramaShot>& shots, double hfovDeg);
    void DrawMatFit(CDC& dc, const cv::Mat& img, const CRect& rcTarget);

    CString GetBaseDir() const;
    CString GetPanoDir() const;
    CString GetIniPath() const;
    CString MakePanoFilePath() const;
    bool SavePanoAndIni(const cv::Mat& pano);
    bool LoadLastPanoramaFromIni();

private:
    cv::Mat m_frame;
    cv::Mat m_panoramaResult;
    std::mutex m_mtx;

    std::vector<PanoramaShot> m_shots;

    CControl_Command* m_pComm = nullptr;
    CVideoView* m_pVideoView = nullptr;

    std::thread m_worker;
    std::atomic<bool> m_bCapturing = false;

    double m_hfovDeg = 30.0;
    double m_vfovDeg = 17.0;
    double m_overlapRatio = 0.35;
    double m_fixedTiltDeg = 0.0;
    double m_panSpeed = 60.0;
    double m_tiltSpeed = 60.0;
    DWORD  m_moveWaitMs = 2000;

    CString m_lastPanoPath;

public:
    CButton m_btn_panomake;
    CButton m_btn_Exit;

    void ClearPreviewFrameOnly();
};