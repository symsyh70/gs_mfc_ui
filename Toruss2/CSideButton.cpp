#include "pch.h"
#include "CSideButton.h"



BEGIN_MESSAGE_MAP(CSideButton, CButton)
    ON_WM_MOUSEMOVE()
    ON_MESSAGE(WM_MOUSELEAVE, &CSideButton::OnMouseLeave)
    ON_WM_LBUTTONDOWN()
    ON_WM_TIMER()
END_MESSAGE_MAP()

void CSideButton::DrawItem(LPDRAWITEMSTRUCT lp)
{


    int pressOffset = 0;
    if (m_bPressed)
        pressOffset = m_nAnimStep;

    CDC dc;
    dc.Attach(lp->hDC);

    CRect rc = lp->rcItem;
    CRect rcDraw = rc;
    rcDraw.OffsetRect(0, pressOffset);

    const COLORREF clrBG = RGB(55, 55, 55);   // 기본
    const COLORREF clrBGHover = RGB(65, 65, 65);   // hover
    const COLORREF clrBGSel = RGB(70, 70, 70);   // 선택
    const COLORREF clrTextSel = RGB(255, 255, 255);// 선택 텍스트(흰색)
    const COLORREF clrTextNorm = RGB(120, 120, 120);// 비선택 텍스트(회색)
    const COLORREF clrShadow = RGB(20, 20, 20);   // 그림자
    const COLORREF clrMint = RGB(1, 254, 206);  // 민트
    const int lineH = 3;                            // underline 두께

    CString txt;
    GetWindowText(txt);

    dc.SetBkMode(TRANSPARENT);

    // 1) 배경
    if (m_bSelected)
    {
        CRect rcShadow = rcDraw;
        rcShadow.OffsetRect(4, 4);
        dc.FillSolidRect(rcShadow, clrShadow);
        dc.FillSolidRect(rcDraw, clrBGSel);
    }
    else if (m_bHover)
        dc.FillSolidRect(rcDraw, clrBGHover);
    else
        dc.FillSolidRect(rcDraw, clrBG);

    // 2) 선택 underline (맨 아래 3px)
    if (m_bSelected)
    {
        CRect rcLine = rcDraw;
        rcLine.top = rcLine.bottom - lineH;
        dc.FillSolidRect(rcLine, clrMint);
    }

    // 3) 텍스트 영역 (underline 공간 피하기)
    CRect rcText = rcDraw;
    rcText.DeflateRect(20, 0, 20, lineH);   // 좌우 여백, 아래 underline 피함

    // 4) 텍스트
    CFont* pOldFont = dc.SelectObject(&m_fontText);

    if (m_bSelected)
    {

        // 본문 흰색
        dc.SetTextColor(clrTextSel);
        dc.DrawText(txt, rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    else
    {
        dc.SetTextColor(clrTextNorm);
        dc.DrawText(txt, rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    dc.SelectObject(pOldFont);

    // (옵션) 기존 화살표/마이너스 표시를 탭에서는 보통 안 씀
    // 만약 유지하고 싶으면 여기서 조건부로 DrawText 해주면 됨.

    dc.Detach();
}

void CSideButton::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_bHover)
    {
        m_bHover = true;

        m_tme.cbSize = sizeof(TRACKMOUSEEVENT);
        m_tme.dwFlags = TME_LEAVE;
        m_tme.hwndTrack = m_hWnd;

        TrackMouseEvent(&m_tme);

        Invalidate(FALSE);
    }

    CButton::OnMouseMove(nFlags, point);
}

LRESULT CSideButton::OnMouseLeave(WPARAM, LPARAM)
{
    m_bHover = false;
    Invalidate(FALSE);

    return 0;
}

void CSideButton::SetSelected(bool bSel)
{
    m_bSelected = bSel;
    Invalidate(FALSE);
}
void CSideButton::OnLButtonDown(UINT nFlags, CPoint point)
{
    m_bPressed = true;
    m_nAnimStep = 0;

    SetTimer(TIMER_PRESS_ANIM, 15, nullptr); // 약 60fps


    CButton::OnLButtonDown(nFlags, point);
}

void CSideButton::PreSubclassWindow()
{
    m_fontText.CreateFontW(
        20,
        0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        L"Pretendard"
    );

    m_fontArrow.CreateFontW(
        24,
        0, 0, 0,
        FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        L"맑은 고딕"
    );

    CButton::PreSubclassWindow();
}

void CSideButton::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_PRESS_ANIM)
    {
        m_nAnimStep++;

        if (m_nAnimStep >= 5)
        {
            KillTimer(TIMER_PRESS_ANIM);

            m_bPressed = false;
            m_nAnimStep = 0;

            GetParent()->SendMessage(WM_COMMAND, GetDlgCtrlID());
        }

        Invalidate(FALSE);
    }

    CButton::OnTimer(nIDEvent);
}
