#include "pch.h"
#include "CConnectButton.h"

BEGIN_MESSAGE_MAP(CConnectButton, CButton)
    ON_WM_MOUSEMOVE()
    ON_MESSAGE(WM_MOUSELEAVE, &CConnectButton::OnMouseLeave)
    ON_WM_LBUTTONDOWN()
    ON_WM_TIMER()
END_MESSAGE_MAP()

void CConnectButton::DrawItem(LPDRAWITEMSTRUCT lp)
{

    CDC* dc = CDC::FromHandle(lp->hDC);
    CRect rc = lp->rcItem;

    dc->FillSolidRect(rc, RGB(49, 49, 49));

    CString text;
    GetWindowText(text);

    dc->SetBkMode(TRANSPARENT);

    COLORREF clr = RGB(140, 140, 140);

    if (m_bHover)
        clr = RGB(200, 200, 200);

    if (m_bSelected)
        clr = RGB(0, 220, 210);

    CFont* pOldFont = dc->SelectObject(&m_fontText);

    dc->SetTextColor(clr);

    dc->DrawText(text, rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    dc->SelectObject(pOldFont);

}

void CConnectButton::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_bHover)
    {
        m_bHover = TRUE;
        Invalidate();

        TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT) };
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hWnd;
        TrackMouseEvent(&tme);
    }

    CButton::OnMouseMove(nFlags, point);
}

LRESULT CConnectButton::OnMouseLeave(WPARAM, LPARAM)
{
    m_bHover = false;
    Invalidate(FALSE);

    return 0;
}

void CConnectButton::SetSelected(bool bSel)
{
    m_bSelected = bSel;
    Invalidate(FALSE);
}
void CConnectButton::OnLButtonDown(UINT nFlags, CPoint point)
{
    m_bPressed = true;
    m_nAnimStep = 0;

    SetTimer(TIMER_PRESS_ANIM, 3, nullptr); // ¾à 60fps


    CButton::OnLButtonDown(nFlags, point);
}

void CConnectButton::PreSubclassWindow()
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
        L"¸¼Àº °íµñ"
    );

    CButton::PreSubclassWindow();
}

void CConnectButton::OnTimer(UINT_PTR nIDEvent)
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
