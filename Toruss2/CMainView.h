#pragma once
#include <afxwin.h>
#include <gdiplus.h>
#include "CControl_Command.h"
#include <string.h>
#include <algorithm>
class CToruss2Dlg;

class CMainVideoView : public CStatic
{
public:
    enum class HIT_DIR
    {
        NONE,
        UP,
        DOWN,
        LEFT,
        RIGHT,
        UP_LEFT,
        UP_RIGHT,
        DOWN_LEFT,
        DOWN_RIGHT  
    };

private:
    bool m_bMiniMapMeasure = false;

public:
    CMainVideoView();
    virtual ~CMainVideoView();

    void SetMainDlg(CToruss2Dlg* pDlg);
    void SetImageFolder(const CString& folderPath);
    void SetVideoActive(bool active);
    void DrawOverlayNow();
    void SetCommManager(CControl_Command* pComm);

protected:
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg LRESULT OnMouseLeave(WPARAM wParam, LPARAM lParam);
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()
public:
    HIT_DIR GetHoverDir() const { return m_hoverDir; }
    CPoint GetLastMousePt() const { return m_lastMousePt; }
    bool IsVideoActive() const { return m_bVideoActive; }
    CPoint m_lastMousePt{ 0, 0 };
private:
    HIT_DIR HitTestDir(CPoint pt) const;
    void TrackMouseLeave();
    void DrawDirectionOverlay(CDC* pDC);
    void SendPTZStart(HIT_DIR dir);
    void SendPTZStop();
    void LoadDirectionImages();
    void ClearDirectionImages();

public:
    bool IsDragging() const { return m_bDragging; }

private:
    bool m_bDragging = false;
    HIT_DIR m_sentDir = HIT_DIR::NONE;
    int m_deadZoneRadius = 180;

private:
    CToruss2Dlg* m_pMainDlg = nullptr;
    CControl_Command* m_pComm = nullptr;

    HIT_DIR m_hoverDir = HIT_DIR::NONE;
    HIT_DIR m_pressedDir = HIT_DIR::NONE;
    bool m_bTrackingMouseLeave = false;
    bool m_bVideoActive = false;


    CString m_imgFolder;

    Gdiplus::Image* m_pImgUp = nullptr;
    Gdiplus::Image* m_pImgDown = nullptr;
    Gdiplus::Image* m_pImgLeft = nullptr;
    Gdiplus::Image* m_pImgRight = nullptr;
    Gdiplus::Image* m_pImgUpLeft = nullptr;
    Gdiplus::Image* m_pImgUpRight = nullptr;
    Gdiplus::Image* m_pImgDownLeft = nullptr;
    Gdiplus::Image* m_pImgDownRight = nullptr;

    Gdiplus::Image* m_pImgPanDeg = nullptr;
    Gdiplus::Image* m_pImgTiltDeg = nullptr;
    Gdiplus::Image* m_pImgTiltDeg_Arrow = nullptr;

    HCURSOR m_hCurDefault = nullptr;
    HCURSOR m_hCurHand = nullptr;
public:
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
};