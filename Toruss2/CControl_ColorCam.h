#pragma once
#include "afxdialogex.h"
#include "CControl_Command.h"

// CControl_ColorCam 대화 상자

class CControl_ColorCam : public CDialogEx
{
	DECLARE_DYNAMIC(CControl_ColorCam)

public:
	CControl_ColorCam(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~CControl_ColorCam();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PANEL_COLORCAM };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	DECLARE_MESSAGE_MAP()

	CControl_Command* m_pComm = nullptr;

public:
	void SetCommManager(CControl_Command* pComm);
	BOOL SendZoomTele();
	BOOL SendZoomWide();
	BOOL SendZoomStop();

	BOOL SendFocusFar();
	BOOL SendFocusNear();
	BOOL SendFocusStop();
	BOOL SendAutoFocus();

	BOOL SendDayMode();
	BOOL SendNightMode();

	int m_nWipeTimerCnt = 0;
	int m_nWiperTime = 1;

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnBnClickedBtnColorAutofocus();
	afx_msg void OnBnClickedBtnColorDaymode();
	afx_msg void OnBnClickedBtnColorNightmode();
	CEdit m_WiperDelayTime;
	afx_msg void OnBnClickedBtnColorWiperUp();
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedBtnColorWiperDown();
	CButton m_btn_wiper;
	afx_msg void OnBnClickedBtnColorWiper();
};
