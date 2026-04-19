#pragma once
#include "afxdialogex.h"
#include "CControl_Command.h"

// CControl_ThermalCam 대화 상자

class CControl_ThermalCam : public CDialogEx
{
	DECLARE_DYNAMIC(CControl_ThermalCam)

public:
	CControl_ThermalCam(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~CControl_ThermalCam();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PANEL_THERMALCAM };
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

	BOOL SendNextPalette();
	BOOL SendPrevPalette();
	BOOL SendGreyAndWhite();
	afx_msg void OnBnClickedBtnThermalPalettePrev();
	afx_msg void OnBnClickedBtnThermalPaletteNext();
	afx_msg void OnBnClickedBtnThermalPaletteWhitegrey();
};
