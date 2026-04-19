#pragma once

#define TIMER_PRESS_ANIM  2001


class CSideButton : public CButton
{
public:
    CSideButton()
    {
        m_bHover = false;
        m_bSelected = false;
    }

    bool m_bSelected;
    bool  m_bPressed = false;
    int   m_nAnimStep = 0;   // 0 ~ 5


protected:
    bool m_bHover;
    TRACKMOUSEEVENT m_tme;

    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

public:
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg LRESULT OnMouseLeave(WPARAM, LPARAM);

    void SetSelected(bool bSel);

    CFont m_fontText;
    CFont m_fontArrow;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    virtual void PreSubclassWindow();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
};
