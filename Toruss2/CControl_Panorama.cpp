#include "pch.h"
#include "Toruss2.h"
#include "afxdialogex.h"
#include "CControl_Panorama.h"
#include <cmath>
#include <algorithm>

BEGIN_MESSAGE_MAP(CControl_Panorama, CDialogEx)
    ON_WM_PAINT()
    ON_WM_NCHITTEST()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_ERASEBKGND()
    ON_BN_CLICKED(IDC_BUTTON_PANOMAKE, &CControl_Panorama::OnBnClickedButtonPanomake)
END_MESSAGE_MAP()

CControl_Panorama::CControl_Panorama(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_DIALOG_PANORAMA, pParent)
{
}

CControl_Panorama::~CControl_Panorama()
{
    m_bCapturing = false;
    if (m_worker.joinable())
        m_worker.join();
}

void CControl_Panorama::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_BUTTON_PANOMAKE, m_btn_panomake);
    DDX_Control(pDX, IDC_BUTTON_EXIT, m_btn_Exit);
}

BOOL CControl_Panorama::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetBackgroundColor(RGB(49, 49, 49));

    LoadLastPanoramaFromIni();

    return TRUE;
}

void CControl_Panorama::SetCommManager(CControl_Command* pComm)
{
    m_pComm = pComm;
}

void CControl_Panorama::SetVideoView(CVideoView* pVideoView)
{
    m_pVideoView = pVideoView;
}

void CControl_Panorama::SetFrame(const cv::Mat& frame)
{
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (frame.empty())
            m_frame.release();
        else
            m_frame = frame.clone();
    }
    Invalidate(FALSE);
}

void CControl_Panorama::ClearFrame()
{
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_frame.release();
        m_panoramaResult.release();
    }
    Invalidate(FALSE);
}

void CControl_Panorama::ClearPreviewFrameOnly()
{
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_frame.release();
    }
    Invalidate(FALSE);
}

double CControl_Panorama::Normalize180(double deg)
{
    while (deg < -180.0)
        deg += 360.0;
    while (deg > 180.0)
        deg -= 360.0;

    if (deg == -180.0)
        deg = 180.0;

    return deg;
}

double CControl_Panorama::Normalize360(double deg)
{
    while (deg < 0.0)
        deg += 360.0;
    while (deg >= 360.0)
        deg -= 360.0;
    return deg;
}

double CControl_Panorama::Pan180ToPan360(double pan180)
{
    pan180 = Normalize180(pan180);
    if (pan180 < 0.0)
        pan180 += 360.0;
    return pan180;
}

double CControl_Panorama::Clamp(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void CControl_Panorama::OnPaint()
{
    CPaintDC dc(this);

    CRect rcClient;
    GetClientRect(&rcClient);

    dc.FillSolidRect(rcClient, RGB(49, 49, 49));

    CRect rcTitle = rcClient;
    rcTitle.bottom = 60;
    dc.FillSolidRect(rcTitle, RGB(34, 34, 34));

    cv::Mat showFrame;
    bool isPanoResult = false;

    {
        std::lock_guard<std::mutex> lock(m_mtx);

        if (!m_panoramaResult.empty())
        {
            showFrame = m_panoramaResult.clone();
            isPanoResult = true;
        }
        else if (m_bCapturing && !m_frame.empty())
        {
            showFrame = m_frame.clone();
        }
    }

    CRect rcView = rcClient;
    rcView.top = 60;
    rcView.left += 2;
    rcView.right -= 2;
    rcView.bottom -= 2;

    if (!showFrame.empty())
    {
        cv::Mat drawFrame = showFrame;

        if (isPanoResult)
        {
            const int panoW = showFrame.cols;
            const int panoH = showFrame.rows;

            double displayHFOV = std::max(m_curHFOV * 4.0, 60.0);
            int viewW = (int)std::round((displayHFOV / 360.0) * panoW);
            viewW = std::max(1, std::min(viewW, panoW));

            const double pan360 = (m_curPan < 0.0) ? (m_curPan + 360.0) : m_curPan;
            int centerX = (int)std::round((pan360 / 360.0) * panoW);
            centerX %= panoW;

            int startX = centerX - (viewW / 2);

            if (startX < 0 || startX + viewW > panoW)
            {
                cv::Mat wrapped;
                cv::hconcat(showFrame, showFrame, wrapped);

                int wrapStartX = startX;
                while (wrapStartX < 0) wrapStartX += panoW;

                cv::Rect roi(wrapStartX, 0, viewW, panoH);
                drawFrame = wrapped(roi).clone();
            }
            else
            {
                cv::Rect roi(startX, 0, viewW, panoH);
                drawFrame = showFrame(roi).clone();
            }
        }

        DrawMatFit(dc, drawFrame, rcView);
    }

    CString status;
    status.Format(
        L"Panorama [%s] HFOV=%.1f Overlap=%.0f%% Shots=%d",
        m_bCapturing ? L"CAPTURING" : L"IDLE",
        m_hfovDeg,
        m_overlapRatio * 100.0,
        (int)m_shots.size());

    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(RGB(235, 235, 235));
    dc.TextOutW(20, 20, status);
}

LRESULT CControl_Panorama::OnNcHitTest(CPoint point)
{
    CPoint pt = point;
    ScreenToClient(&pt);

    if (pt.y < 60)
    {
        PostMessage(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
        return 0;
    }

    return CDialogEx::OnNcHitTest(point);
}

void CControl_Panorama::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    if (!::IsWindow(m_btn_panomake.GetSafeHwnd()) || !::IsWindow(m_btn_Exit.GetSafeHwnd()))
        return;

    const int top = 12;
    const int btnW = 120;
    const int btnH = 36;
    const int gap = 10;

    m_btn_Exit.MoveWindow(cx - btnW - 20, top, btnW, btnH);
    m_btn_panomake.MoveWindow(cx - btnW * 2 - gap - 20, top, btnW, btnH);
}

void CControl_Panorama::OnLButtonDown(UINT nFlags, CPoint point)
{
    CDialogEx::OnLButtonDown(nFlags, point);
}

BOOL CControl_Panorama::OnEraseBkgnd(CDC* pDC)
{
    UNREFERENCED_PARAMETER(pDC);
    return TRUE;
}

void CControl_Panorama::OnBnClickedButtonPanomake()
{
    if (m_bCapturing)
        return;

    if (m_pComm == nullptr || m_pVideoView == nullptr)
    {
        AfxMessageBox(L"Comm 또는 VideoView 연결이 안됨");
        return;
    }

    if (m_hfovDeg <= 1.0 || m_hfovDeg >= 180.0)
    {
        AfxMessageBox(L"HFOV 값이 이상함. 예: 30.0");
        return;
    }

    m_bCapturing = true;
    m_shots.clear();

    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_panoramaResult.release();
        m_frame.release();
    }

    if (m_worker.joinable())
        m_worker.join();

    m_worker = std::thread(&CControl_Panorama::RunPanoramaCapture360, this);
}

void CControl_Panorama::RunPanoramaCapture360()
{
    try
    {
        const double overlap = Clamp(m_overlapRatio, 0.05, 0.85);
        const double stepDeg = m_hfovDeg * (1.0 - overlap);
        const double finalStepDeg = Clamp(stepDeg, 5.0, 45.0);

        m_pComm->SendCCB(0x00, 0x4D, 0x02, 0x00); // shortest path
        m_pComm->SetTiltSpeed(m_tiltSpeed);
        m_pComm->TiltGoPosition(m_fixedTiltDeg);
        ::Sleep(1000);

        m_pComm->SetPanSpeed(m_panSpeed);
        m_pComm->PanGoPosition(-179.9);
        ::Sleep(5000);

        for (double panCmd180 = -180.0; panCmd180 <= 180.0; panCmd180 += finalStepDeg)
        {
            const double panNorm180 = Normalize180(panCmd180);
            const double panForPano360 = Pan180ToPan360(panNorm180);

            CString dbg;
            dbg.Format(L"[PANO] cmd180=%.2f -> pano360=%.2f\r\n", panNorm180, panForPano360);
            OutputDebugString(dbg);

            m_pComm->PanGoPosition(panNorm180);
            ::Sleep(m_moveWaitMs);

            cv::Mat best;
            if (CaptureBestFrame(best, 5, 80) && !best.empty())
            {
                PanoramaShot shot;
                shot.panDeg = panForPano360;
                shot.tiltDeg = m_fixedTiltDeg;
                shot.frame = best;
                m_shots.push_back(shot);

                SetFrame(best);
            }
        }

        m_pComm->PanGoPosition(0.0);
        ::Sleep(m_moveWaitMs);

        cv::Mat best0;
        if (CaptureBestFrame(best0, 5, 80) && !best0.empty())
        {
            PanoramaShot shot;
            shot.panDeg = 0.0;
            shot.tiltDeg = m_fixedTiltDeg;
            shot.frame = best0;
            m_shots.push_back(shot);
        }

        cv::Mat pano = BuildPanorama360(m_shots, m_hfovDeg);

        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_panoramaResult = pano.clone();
            m_frame = pano.clone();
        }

        SavePanoAndIni(pano);
        Invalidate(FALSE);
    }
    catch (...)
    {
    }

    m_bCapturing = false;
    Invalidate(FALSE);
}

bool CControl_Panorama::CaptureBestFrame(cv::Mat& outFrame, int sampleCount, int intervalMs)
{
    double bestScore = -1.0;
    cv::Mat bestFrame;

    for (int i = 0; i < sampleCount; ++i)
    {
        cv::Mat frame;
        if (m_pVideoView->GetLatestColorFrame(frame) && !frame.empty())
        {
            double score = CalcSharpness(frame);
            if (score > bestScore)
            {
                bestScore = score;
                bestFrame = frame.clone();
            }
        }
        ::Sleep(intervalMs);
    }

    if (bestFrame.empty())
        return false;

    outFrame = bestFrame;
    return true;
}

double CControl_Panorama::CalcSharpness(const cv::Mat& img)
{
    if (img.empty())
        return 0.0;

    cv::Mat gray;
    if (img.channels() == 3)
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    else if (img.channels() == 4)
        cv::cvtColor(img, gray, cv::COLOR_BGRA2GRAY);
    else
        gray = img;

    cv::Mat lap;
    cv::Laplacian(gray, lap, CV_64F);

    cv::Scalar mean, stddev;
    cv::meanStdDev(lap, mean, stddev);
    return stddev[0] * stddev[0];
}

cv::Mat CControl_Panorama::CylindricalWarp(const cv::Mat& src, double hfovDeg)
{
    if (src.empty())
        return cv::Mat();

    cv::Mat bgr;
    if (src.channels() == 4)
        cv::cvtColor(src, bgr, cv::COLOR_BGRA2BGR);
    else if (src.channels() == 1)
        cv::cvtColor(src, bgr, cv::COLOR_GRAY2BGR);
    else
        bgr = src;

    cv::Mat dst = cv::Mat::zeros(bgr.size(), CV_8UC3);

    const double w = (double)bgr.cols;
    const double h = (double)bgr.rows;
    const double cx = w * 0.5;
    const double cy = h * 0.5;

    const double hfovRad = hfovDeg * CV_PI / 180.0;
    const double f = w / (2.0 * tan(hfovRad * 0.5));

    for (int y = 0; y < dst.rows; ++y)
    {
        for (int x = 0; x < dst.cols; ++x)
        {
            double theta = (x - cx) / f;
            double srcX = f * tan(theta) + cx;
            double nx = (srcX - cx) / f;
            double srcY = (y - cy) * sqrt(nx * nx + 1.0) + cy;

            int ix = (int)std::round(srcX);
            int iy = (int)std::round(srcY);

            if (ix >= 0 && ix < bgr.cols && iy >= 0 && iy < bgr.rows)
                dst.at<cv::Vec3b>(y, x) = bgr.at<cv::Vec3b>(iy, ix);
        }
    }

    return dst;
}

void CControl_Panorama::BlendShot360(
    const cv::Mat& warped,
    double panDeg,
    double hfovDeg,
    cv::Mat& accum,
    cv::Mat& weight)
{
    if (warped.empty())
        return;

    const int panoW = accum.cols;
    const int panoH = accum.rows;
    const double pxPerDeg = (double)panoW / 360.0;

    const double usableRatio = 0.70;
    const double effectiveHFOV = hfovDeg * usableRatio;

    int cropW = (int)std::round(warped.cols * usableRatio);
    cropW = std::max(1, std::min(cropW, warped.cols));
    int cropX = (warped.cols - cropW) / 2;

    cv::Mat cropped = warped(cv::Rect(cropX, 0, cropW, warped.rows)).clone();

    const double centerX = Normalize360(panDeg) * pxPerDeg;
    const double shotW = effectiveHFOV * pxPerDeg;
    const double startX = centerX - shotW * 0.5;

    cv::Mat resized;
    cv::resize(cropped, resized,
        cv::Size((int)std::round(shotW), panoH),
        0, 0, cv::INTER_LINEAR);

    if (resized.empty())
        return;

    const int edge = std::max(1, resized.cols / 6);

    for (int y = 0; y < panoH; ++y)
    {
        const cv::Vec3b* srcRow = resized.ptr<cv::Vec3b>(y);

        for (int x = 0; x < resized.cols; ++x)
        {
            int dstX = (int)std::round(startX + x);

            while (dstX < 0) dstX += panoW;
            while (dstX >= panoW) dstX -= panoW;

            const cv::Vec3b& s = srcRow[x];

            if (s[0] == 0 && s[1] == 0 && s[2] == 0)
                continue;

            float alpha = 1.0f;

            if (x < edge)
                alpha = (float)x / (float)edge;
            else if (x > resized.cols - edge - 1)
                alpha = (float)(resized.cols - 1 - x) / (float)edge;

            alpha = std::max(0.0f, std::min(1.0f, alpha));

            cv::Vec3f& d = accum.at<cv::Vec3f>(y, dstX);
            float& w = weight.at<float>(y, dstX);

            d[0] += s[0] * alpha;
            d[1] += s[1] * alpha;
            d[2] += s[2] * alpha;
            w += alpha;
        }
    }
}

cv::Mat CControl_Panorama::BuildPanorama360(const std::vector<PanoramaShot>& shots, double hfovDeg)
{
    if (shots.empty())
        return cv::Mat();

    const int panoH = 720;
    const int panoW = 8192;

    cv::Mat accum(panoH, panoW, CV_32FC3, cv::Scalar(0, 0, 0));
    cv::Mat weight(panoH, panoW, CV_32F, cv::Scalar(0));

    for (const auto& shot : shots)
    {
        if (shot.frame.empty())
            continue;

        cv::Mat src;
        if (shot.frame.channels() == 4)
            cv::cvtColor(shot.frame, src, cv::COLOR_BGRA2BGR);
        else if (shot.frame.channels() == 1)
            cv::cvtColor(shot.frame, src, cv::COLOR_GRAY2BGR);
        else
            src = shot.frame;

        double scale = (double)panoH / (double)src.rows;
        cv::Mat resized;
        cv::resize(src, resized, cv::Size(), scale, scale, cv::INTER_LINEAR);

        cv::Mat warped = CylindricalWarp(resized, hfovDeg);
        BlendShot360(warped, shot.panDeg, hfovDeg, accum, weight);
    }

    cv::Mat result(panoH, panoW, CV_8UC3, cv::Scalar(0, 0, 0));

    for (int y = 0; y < panoH; ++y)
    {
        for (int x = 0; x < panoW; ++x)
        {
            float w = weight.at<float>(y, x);
            if (w > 0.0001f)
            {
                cv::Vec3f v = accum.at<cv::Vec3f>(y, x) / w;
                result.at<cv::Vec3b>(y, x) = cv::Vec3b(
                    cv::saturate_cast<uchar>(v[0]),
                    cv::saturate_cast<uchar>(v[1]),
                    cv::saturate_cast<uchar>(v[2]));
            }
        }
    }

    const int seam = 64;
    for (int y = 0; y < result.rows; ++y)
    {
        for (int i = 0; i < seam; ++i)
        {
            float a = (float)i / (float)seam;
            cv::Vec3b left = result.at<cv::Vec3b>(y, i);
            cv::Vec3b right = result.at<cv::Vec3b>(y, result.cols - seam + i);

            cv::Vec3b mix;
            for (int c = 0; c < 3; ++c)
                mix[c] = cv::saturate_cast<uchar>(left[c] * a + right[c] * (1.0f - a));

            result.at<cv::Vec3b>(y, i) = mix;
            result.at<cv::Vec3b>(y, result.cols - seam + i) = mix;
        }
    }

    return result;
}

void CControl_Panorama::DrawMatFit(CDC& dc, const cv::Mat& img, const CRect& rcTarget)
{
    if (img.empty())
        return;

    cv::Mat bgr;
    if (img.channels() == 1)
        cv::cvtColor(img, bgr, cv::COLOR_GRAY2BGR);
    else if (img.channels() == 4)
        cv::cvtColor(img, bgr, cv::COLOR_BGRA2BGR);
    else
        bgr = img;

    // 공백 없이 꽉 채우기: cover 방식
    double sx = (double)rcTarget.Width() / (double)bgr.cols;
    double sy = (double)rcTarget.Height() / (double)bgr.rows;
    double scale = std::max(sx, sy);   // min -> max 로 바뀜

    int drawW = (int)std::round(bgr.cols * scale);
    int drawH = (int)std::round(bgr.rows * scale);

    if (drawW <= 0 || drawH <= 0)
        return;

    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(drawW, drawH), 0, 0, cv::INTER_LINEAR);

    // 가운데 기준 crop
    int cropX = 0;
    int cropY = 0;

    if (drawW > rcTarget.Width())
        cropX = (drawW - rcTarget.Width()) / 2;

    if (drawH > rcTarget.Height())
        cropY = (drawH - rcTarget.Height()) / 2;

    cv::Rect roi(cropX, cropY, rcTarget.Width(), rcTarget.Height());
    cv::Mat cropped = resized(roi).clone();

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cropped.cols;
    bmi.bmiHeader.biHeight = -cropped.rows;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    ::SetDIBitsToDevice(
        dc.GetSafeHdc(),
        rcTarget.left, rcTarget.top,
        cropped.cols, cropped.rows,
        0, 0, 0, cropped.rows,
        cropped.data,
        &bmi,
        DIB_RGB_COLORS);
}

CString CControl_Panorama::GetBaseDir() const
{
    wchar_t path[MAX_PATH] = { 0 };
    ::GetModuleFileNameW(nullptr, path, MAX_PATH);

    CString s(path);
    int pos = s.ReverseFind(L'\\');
    if (pos >= 0)
        s = s.Left(pos);

    return s;
}

CString CControl_Panorama::GetPanoDir() const
{
    CString dir = GetBaseDir() + L"\\Panorama";
    ::CreateDirectoryW(dir, nullptr);
    return dir;
}

CString CControl_Panorama::GetIniPath() const
{
    return GetPanoDir() + L"\\panorama.ini";
}

CString CControl_Panorama::MakePanoFilePath() const
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);

    CString path;
    path.Format(L"%s\\Pano_%04d%02d%02d_%02d%02d%02d.png",
        GetPanoDir().GetString(),
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);

    return path;
}

bool CControl_Panorama::SavePanoAndIni(const cv::Mat& pano)
{
    if (pano.empty())
        return false;

    CString filePath = MakePanoFilePath();

    CStringA filePathA(filePath);
    bool ok = cv::imwrite(std::string(filePathA), pano);

    if (!ok)
        return false;

    ::WritePrivateProfileStringW(L"Panorama", L"LastFile", filePath, GetIniPath());

    m_lastPanoPath = filePath;
    return true;
}

bool CControl_Panorama::LoadLastPanoramaFromIni()
{
    wchar_t buf[MAX_PATH * 4] = { 0 };
    ::GetPrivateProfileStringW(
        L"Panorama",
        L"LastFile",
        L"",
        buf,
        (DWORD)_countof(buf),
        GetIniPath());

    CString path(buf);
    if (path.IsEmpty())
        return false;

    CStringA pathA(path);
    cv::Mat pano = cv::imread(std::string(pathA), cv::IMREAD_COLOR);
    if (pano.empty())
        return false;

    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_panoramaResult = pano.clone();
        m_frame = pano.clone();
    }

    m_lastPanoPath = path;
    Invalidate(FALSE);
    return true;
}

void CControl_Panorama::SetCurrentView(double pan, double tilt, double hfov, double vfov)
{
    pan = Normalize180(pan);

    if (fabs(m_curPan - pan) < 0.3 &&
        fabs(m_curTilt - tilt) < 0.3 &&
        fabs(m_curHFOV - hfov) < 0.2)
    {
        return;
    }

    m_curPan = pan;
    m_curTilt = tilt;
    m_curHFOV = hfov;
    m_curVFOV = vfov;

    Invalidate(FALSE);
} 