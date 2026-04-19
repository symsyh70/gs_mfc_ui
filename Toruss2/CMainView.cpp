#include "pch.h"
#include "CMainView.h"
#include "Toruss2Dlg.h"

#ifndef WM_MOUSELEAVE
#define WM_MOUSELEAVE 0x02A3
#endif

BEGIN_MESSAGE_MAP(CMainVideoView, CStatic)
    ON_WM_MOUSEMOVE()
    ON_WM_SETCURSOR()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_MESSAGE(WM_MOUSELEAVE, &CMainVideoView::OnMouseLeave)
    ON_WM_PAINT()
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

CMainVideoView::CMainVideoView()
{
    m_hCurDefault = ::LoadCursor(nullptr, IDC_ARROW);
    m_hCurHand = ::LoadCursor(nullptr, IDC_HAND);
}

CMainVideoView::~CMainVideoView()
{
    ClearDirectionImages();
}

void CMainVideoView::SetMainDlg(CToruss2Dlg* pDlg)
{
    m_pMainDlg = pDlg;
}

void CMainVideoView::SetVideoActive(bool active)
{
    if (m_bVideoActive == active)
        return;

    m_bVideoActive = active;

    if (!m_bVideoActive)
    {
        m_hoverDir = HIT_DIR::NONE;
        m_pressedDir = HIT_DIR::NONE;
    }

}

void CMainVideoView::ClearDirectionImages()
{
    delete m_pImgUp;    m_pImgUp = nullptr;
    delete m_pImgDown;  m_pImgDown = nullptr;
    delete m_pImgLeft;  m_pImgLeft = nullptr;
    delete m_pImgRight; m_pImgRight = nullptr;
}

void CMainVideoView::SetImageFolder(const CString& folderPath)
{
    m_imgFolder = folderPath;
    LoadDirectionImages();
    Invalidate(FALSE);
}

void CMainVideoView::LoadDirectionImages()
{
    ClearDirectionImages();

    if (m_imgFolder.IsEmpty())
        return;

    CString up = m_imgFolder + L"\\up.png";
    CString down = m_imgFolder + L"\\down.png";
    CString left = m_imgFolder + L"\\left.png";
    CString right = m_imgFolder + L"\\right.png";
    CString upleft = m_imgFolder + L"\\up-left.png";
    CString upright = m_imgFolder + L"\\up-right.png";
    CString downleft = m_imgFolder + L"\\down-left.png";
    CString downright = m_imgFolder + L"\\down-right.png";

    CString TiltDeg_Arrow = m_imgFolder + L"\\TiltDeg_Arrow.png";
    CString TiltDeg = m_imgFolder + L"\\TiltDegBk.png";
    CString PanDeg = m_imgFolder + L"\\Compas.png";

    m_pImgTiltDeg_Arrow = Gdiplus::Image::FromFile(TiltDeg_Arrow);
    m_pImgTiltDeg = Gdiplus::Image::FromFile(TiltDeg);
    m_pImgPanDeg = Gdiplus::Image::FromFile(PanDeg);

    m_pImgUp = Gdiplus::Image::FromFile(up);
    m_pImgDown = Gdiplus::Image::FromFile(down);
    m_pImgLeft = Gdiplus::Image::FromFile(left);
    m_pImgRight = Gdiplus::Image::FromFile(right);

    m_pImgUpLeft = Gdiplus::Image::FromFile(upleft);
    m_pImgUpRight = Gdiplus::Image::FromFile(upright);
    m_pImgDownLeft = Gdiplus::Image::FromFile(downleft);
    m_pImgDownRight = Gdiplus::Image::FromFile(downright);


    if (m_pImgTiltDeg_Arrow && m_pImgTiltDeg_Arrow->GetLastStatus() != Gdiplus::Ok) { delete m_pImgTiltDeg_Arrow; m_pImgTiltDeg_Arrow = nullptr; }
    if (m_pImgTiltDeg && m_pImgTiltDeg->GetLastStatus() != Gdiplus::Ok) { delete m_pImgTiltDeg; m_pImgTiltDeg = nullptr; }
    if (m_pImgPanDeg && m_pImgPanDeg->GetLastStatus() != Gdiplus::Ok) { delete m_pImgPanDeg; m_pImgPanDeg = nullptr; }

    if (m_pImgUp && m_pImgUp->GetLastStatus() != Gdiplus::Ok) { delete m_pImgUp; m_pImgUp = nullptr; }
    if (m_pImgDown && m_pImgDown->GetLastStatus() != Gdiplus::Ok) { delete m_pImgDown; m_pImgDown = nullptr; }
    if (m_pImgLeft && m_pImgLeft->GetLastStatus() != Gdiplus::Ok) { delete m_pImgLeft; m_pImgLeft = nullptr; }
    if (m_pImgRight && m_pImgRight->GetLastStatus() != Gdiplus::Ok) { delete m_pImgRight; m_pImgRight = nullptr; }
    if (m_pImgUpLeft && m_pImgUpLeft->GetLastStatus() != Gdiplus::Ok) { delete m_pImgUpLeft; m_pImgUpLeft = nullptr; }
    if (m_pImgUpRight && m_pImgUpRight->GetLastStatus() != Gdiplus::Ok) { delete m_pImgUpRight; m_pImgUpRight = nullptr; }
    if (m_pImgDownLeft && m_pImgDownLeft->GetLastStatus() != Gdiplus::Ok) { delete m_pImgDownLeft; m_pImgDownLeft = nullptr; }
    if (m_pImgDownRight && m_pImgDownRight->GetLastStatus() != Gdiplus::Ok) { delete m_pImgDownRight; m_pImgDownRight = nullptr; }
}

void CMainVideoView::TrackMouseLeave()
{
    if (m_bTrackingMouseLeave)
        return;

    TRACKMOUSEEVENT tme{};
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hWnd;
    ::TrackMouseEvent(&tme);

    m_bTrackingMouseLeave = true;
}

CMainVideoView::HIT_DIR CMainVideoView::HitTestDir(CPoint pt) const
{
    if (!m_bVideoActive)
        return HIT_DIR::NONE;

    CRect rc;
    GetClientRect(&rc);

    int cx = rc.Width() / 2;
    int cy = rc.Height() / 2;

    double dx = pt.x - cx;
    double dy = cy - pt.y;   // 화면 좌표 뒤집기

    double dist = std::sqrt(dx * dx + dy * dy);
    if (dist < m_deadZoneRadius)
        return HIT_DIR::NONE;

    double angle = std::atan2(dy, dx) * 180.0 / 3.14159265358979323846;

    if (angle < 0)
        angle += 360.0;

    if (angle >= 337.5 || angle < 22.5)   return HIT_DIR::RIGHT;
    if (angle < 67.5)                     return HIT_DIR::UP_RIGHT;
    if (angle < 112.5)                    return HIT_DIR::UP;
    if (angle < 157.5)                    return HIT_DIR::UP_LEFT;
    if (angle < 202.5)                    return HIT_DIR::LEFT;
    if (angle < 247.5)                    return HIT_DIR::DOWN_LEFT;
    if (angle < 292.5)                    return HIT_DIR::DOWN;
    return HIT_DIR::DOWN_RIGHT;
}
LRESULT CMainVideoView::OnMouseLeave(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    m_bTrackingMouseLeave = false;
    m_hoverDir = HIT_DIR::NONE;

  /*  if (m_pMainDlg && m_pMainDlg->m_VideoView.IsMiniMapMain())
    {
        m_bDragging = false;
        m_bMiniMapMeasure = false;
        m_sentDir = HIT_DIR::NONE;
        return 0;
    }*/

    if (m_bDragging || m_sentDir != HIT_DIR::NONE)
    {
        SendPTZStop();
        m_sentDir = HIT_DIR::NONE;
        m_bDragging = false;
    }

    return 0;
}
BOOL CMainVideoView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    UNREFERENCED_PARAMETER(pWnd);
    UNREFERENCED_PARAMETER(nHitTest);
    UNREFERENCED_PARAMETER(message);

    if (m_bVideoActive && m_hoverDir != HIT_DIR::NONE)
    {
        ::SetCursor(m_hCurHand ? m_hCurHand : m_hCurDefault);
        return TRUE;
    }

    ::SetCursor(m_hCurDefault);
    return TRUE;
}

void CMainVideoView::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (m_pMainDlg)
        m_pMainDlg->CancelJogTracking();

    if (!m_bVideoActive)
    {
        CStatic::OnLButtonDown(nFlags, point);
        return;
    }

    SetFocus();
    SetCapture();

    m_bDragging = true;
    m_lastMousePt = point;

    // 메인뷰가 미니맵이면 PTZ 대신 미니맵 조작
    //if (m_pMainDlg && m_pMainDlg->m_VideoView.IsMiniMapMain())
    //{
    //    CRect rc;
    //    GetClientRect(&rc);

    //    // Shift + 클릭 : 거리 측정 시작
    //    if ((::GetKeyState(VK_SHIFT) & 0x8000) != 0)
    //    {
    //        m_bMiniMapMeasure = m_pMainDlg->m_VideoView.BeginMiniMapMeasure(
    //            point.x, point.y, rc.Width(), rc.Height());

    //        if (m_bMiniMapMeasure)
    //            m_bDragging = false;
    //    }
    //    else
    //    {
    //        m_bMiniMapMeasure = false;
    //    }

    //    m_hoverDir = HIT_DIR::NONE;
    //    m_sentDir = HIT_DIR::NONE;
    //    CStatic::OnLButtonDown(nFlags, point);
    //    return;
    //}

    HIT_DIR dir = HitTestDir(point);
    m_hoverDir = dir;
    m_sentDir = HIT_DIR::NONE;

    if (dir != HIT_DIR::NONE)
    {
        SendPTZStart(dir);
        m_sentDir = dir;
    }

    CStatic::OnLButtonDown(nFlags, point);
}

void CMainVideoView::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (GetCapture() == this)
        ReleaseCapture();

    // 메인뷰가 미니맵이면 미니맵 조작 종료
    //if (m_pMainDlg && m_pMainDlg->m_VideoView.IsMiniMapMain())
    //{
    //    CRect rc;
    //    GetClientRect(&rc);

    //    if (m_bMiniMapMeasure)
    //    {
    //        m_bMiniMapMeasure = false;
    //        m_pMainDlg->m_VideoView.EndMiniMapMeasure();
    //    }
    //    else
    //    {
    //        // Ctrl + 클릭 해제 시 해당 지점으로 PTZ 이동
    //        if ((::GetKeyState(VK_CONTROL) & 0x8000) != 0)
    //        {
    //            m_pMainDlg->m_VideoView.MovePTZToMiniMapPoint(
    //                point.x, point.y, rc.Width(), rc.Height());
    //        }
    //    }

    //    m_bDragging = false;
    //    m_sentDir = HIT_DIR::NONE;
    //    CStatic::OnLButtonUp(nFlags, point);
    //    return;
    //}

    //m_bDragging = false;

    if (m_sentDir != HIT_DIR::NONE)
    {
        SendPTZStop();
        m_sentDir = HIT_DIR::NONE;
    }

    CStatic::OnLButtonUp(nFlags, point);
}

void CMainVideoView::SendPTZStart(HIT_DIR dir)
{
    if (!m_pMainDlg || !m_pComm)
        return;

    BYTE spd = static_cast<BYTE>(m_pMainDlg->GetManualPTZSpeed());

    switch (dir)
    {
    case HIT_DIR::LEFT:
        m_pComm->SendCCB(0x00, 0x04, spd, spd);
        break;
    case HIT_DIR::RIGHT:
        m_pComm->SendCCB(0x00, 0x02, spd, spd);
        break;
    case HIT_DIR::UP:
        m_pComm->SendCCB(0x00, 0x08, spd, spd);
        break;
    case HIT_DIR::DOWN:
        m_pComm->SendCCB(0x00, 0x10, spd, spd);
        break;
    case HIT_DIR::UP_LEFT:
        m_pComm->SendCCB(0x00, 0x0C, spd, spd);
        break;
    case HIT_DIR::UP_RIGHT:
        m_pComm->SendCCB(0x00, 0x0A, spd, spd);
        break;
    case HIT_DIR::DOWN_LEFT:
        m_pComm->SendCCB(0x00, 0x14, spd, spd);
        break;
    case HIT_DIR::DOWN_RIGHT:
        m_pComm->SendCCB(0x00, 0x12, spd, spd);
        break;
    default:
        break;
    }
}

void CMainVideoView::SendPTZStop()
{
    if (!m_pMainDlg || !m_pComm)
        return;

    m_pComm->SendCCB(0x00, 0x00, 0x0C, 0x0C);
}

void CMainVideoView::DrawDirectionOverlay(CDC* pDC)
{
    if (!pDC || !m_bVideoActive || m_hoverDir == HIT_DIR::NONE)
        return;

    CRect rc;
    GetClientRect(&rc);

    const int size = 64;
    const int margin = 20;

    int cx = rc.Width() / 2;
    int cy = rc.Height() / 2;

    double dx = m_lastMousePt.x - cx;
    double dy = m_lastMousePt.y - cy;

    double len = sqrt(dx * dx + dy * dy);
    if (len < 1)
        len = 1;

    dx /= len;
    dy /= len;

    double radius = std::min(rc.Width(), rc.Height()) * 0.35;

    int drawX = (int)(cx + dx * radius - size / 2);
    int drawY = (int)(cy + dy * radius - size / 2);

    drawX = std::max(margin, std::min(drawX, (int)rc.right - size - margin));
    drawY = std::max(margin, std::min(drawY, (int)rc.bottom - size - margin));

    Gdiplus::Graphics g(pDC->GetSafeHdc());
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

    Gdiplus::Image* pImg = nullptr;

    switch (m_hoverDir)
    {
    case HIT_DIR::UP:         pImg = m_pImgUp; break;
    case HIT_DIR::DOWN:       pImg = m_pImgDown; break;
    case HIT_DIR::LEFT:       pImg = m_pImgLeft; break;
    case HIT_DIR::RIGHT:      pImg = m_pImgRight; break;
    case HIT_DIR::UP_LEFT:    pImg = m_pImgUpLeft; break;
    case HIT_DIR::UP_RIGHT:   pImg = m_pImgUpRight; break;
    case HIT_DIR::DOWN_LEFT:  pImg = m_pImgDownLeft; break;
    case HIT_DIR::DOWN_RIGHT: pImg = m_pImgDownRight; break;
    }

    if (pImg)
        g.DrawImage(pImg, drawX, drawY, size, size);
}

void CMainVideoView::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_bVideoActive)
    {
        CStatic::OnMouseMove(nFlags, point);
        return;
    }

    TrackMouseLeave();

    //// 메인뷰가 미니맵이면 드래그로 맵 이동 / 측정 / hover 처리
    //if (m_pMainDlg && m_pMainDlg->m_VideoView.IsMiniMapMain())
    //{
    //    CRect rc;
    //    GetClientRect(&rc);

    //    if (m_bMiniMapMeasure)
    //    {
    //        m_pMainDlg->m_VideoView.UpdateMiniMapMeasure(
    //            point.x, point.y, rc.Width(), rc.Height());
    //    }
    //    else if ((nFlags & MK_LBUTTON) && m_bDragging)
    //    {
    //        int dx = point.x - m_lastMousePt.x;
    //        int dy = point.y - m_lastMousePt.y;

    //        m_pMainDlg->m_VideoView.PanMiniMapByViewDelta(
    //            dx, dy, rc.Width(), rc.Height());
    //    }

    //    m_pMainDlg->m_VideoView.SetMiniMapHoverPoint(
    //        point.x, point.y, rc.Width(), rc.Height());

    //    m_lastMousePt = point;
    //    CStatic::OnMouseMove(nFlags, point);
    //    return;
    //}

    m_lastMousePt = point;

    HIT_DIR dir = HitTestDir(point);
    if (dir != m_hoverDir)
        m_hoverDir = dir;

    if (m_bDragging && GetCapture() == this)
    {
        if (dir != m_sentDir)
        {
            if (m_sentDir != HIT_DIR::NONE)
                SendPTZStop();

            if (dir != HIT_DIR::NONE)
                SendPTZStart(dir);

            m_sentDir = dir;
        }
    }

    CStatic::OnMouseMove(nFlags, point);
}

void CMainVideoView::OnPaint()
{
//{
//    CPaintDC dc(this);
//
//    DefWindowProc(WM_PAINT, (WPARAM)dc.m_hDC, 0);
//    DrawDirectionOverlay(&dc);
}


void CMainVideoView::DrawOverlayNow()
{/*
    Invalidate(FALSE);*/
}

void CMainVideoView::SetCommManager(CControl_Command* pComm)
{
    m_pComm = pComm;
}

BOOL CMainVideoView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    UNREFERENCED_PARAMETER(nFlags);

    //if (!m_pMainDlg || !m_pMainDlg->m_VideoView.IsMiniMapMain())
    //    return CStatic::OnMouseWheel(nFlags, zDelta, pt);

    ScreenToClient(&pt);

    CRect rc;
    GetClientRect(&rc);

    m_pMainDlg->m_VideoView.ZoomMiniMapAtPoint(
        pt.x, pt.y,
        rc.Width(), rc.Height(),
        zDelta);

    return TRUE;
}
