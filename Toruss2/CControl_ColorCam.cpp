// CControl_ColorCam.cpp: 구현 파일
//

#include "pch.h"
#include "Toruss2.h"
#include "afxdialogex.h"
#include "CControl_ColorCam.h"

#define TIMER_WIPER	4000

// CControl_ColorCam 대화 상자

IMPLEMENT_DYNAMIC(CControl_ColorCam, CDialogEx)

CControl_ColorCam::CControl_ColorCam(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_PANEL_COLORCAM, pParent)
{

}

CControl_ColorCam::~CControl_ColorCam()
{
}

void CControl_ColorCam::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_WIPER_DELAY_VALUE, m_WiperDelayTime);
	DDX_Control(pDX, IDC_BTN_COLOR_WIPER, m_btn_wiper);
}


BEGIN_MESSAGE_MAP(CControl_ColorCam, CDialogEx)
//	ON_WM_LBUTTONDOWN()
//	ON_WM_LBUTTONUP()
ON_BN_CLICKED(IDC_BTN_COLOR_AUTOFOCUS, &CControl_ColorCam::OnBnClickedBtnColorAutofocus)
ON_BN_CLICKED(IDC_BTN_COLOR_DAYMODE, &CControl_ColorCam::OnBnClickedBtnColorDaymode)
ON_BN_CLICKED(IDC_BTN_COLOR_NIGHTMODE, &CControl_ColorCam::OnBnClickedBtnColorNightmode)
ON_BN_CLICKED(IDC_BTN_COLOR_WIPER_UP, &CControl_ColorCam::OnBnClickedBtnColorWiperUp)
ON_BN_CLICKED(IDC_BTN_COLOR_WIPER_DOWN, &CControl_ColorCam::OnBnClickedBtnColorWiperDown)
ON_BN_CLICKED(IDC_BTN_COLOR_WIPER, &CControl_ColorCam::OnBnClickedBtnColorWiper)
ON_WM_TIMER()
END_MESSAGE_MAP()


// CControl_ColorCam 메시지 처리기

BOOL CControl_ColorCam::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_nWiperTime = 1;
	m_WiperDelayTime.SetWindowTextW(L"1");

	return TRUE;
}

BOOL CControl_ColorCam::SendZoomTele()
{
	return m_pComm->SendCCB(0x00, 0x20, 0x00, 0x00);

}

BOOL CControl_ColorCam::SendZoomWide()
{
	return m_pComm->SendCCB(0x00, 0x40, 0x00, 0x00);
}

BOOL CControl_ColorCam::SendZoomStop()
{
	return m_pComm->SendCCB(0x00, 0x00, 0x00, 0x00);
}

BOOL CControl_ColorCam::SendFocusFar()
{
	return m_pComm->SendCCB(0x00, 0x80, 0x00, 0x00);

}

BOOL CControl_ColorCam::SendFocusNear()
{
	return m_pComm->SendCCB(0x01, 0x00, 0x00, 0x00);
}

BOOL CControl_ColorCam::SendFocusStop()
{
	return m_pComm->SendCCB(0x00, 0x00, 0x00, 0x00);
}

BOOL CControl_ColorCam::SendAutoFocus()
{
	return m_pComm->SendCCB(0x00, 0x35, 0xAF, 0x00);
}

BOOL CControl_ColorCam::SendDayMode()
{
	return m_pComm->SendCCB(0x00, 0x35, 0x00, 0x00);
}

BOOL CControl_ColorCam::SendNightMode()
{
	return m_pComm->SendCCB(0x00, 0x35, 0x01, 0x00);
}


void CControl_ColorCam::SetCommManager(CControl_Command* pComm)
{
	m_pComm = pComm;
}

BOOL CControl_ColorCam::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->hwnd == ::GetDlgItem(m_hWnd, IDC_BTN_COLOR_ZOOM_IN))
	{
		if (pMsg->message == WM_LBUTTONDOWN)
		{
			SendZoomTele();
			return TRUE;
		}
		else if (pMsg->message == WM_LBUTTONUP)
		{
			SendZoomStop();
			return TRUE;
		}
	}
	else if (pMsg->hwnd == ::GetDlgItem(m_hWnd, IDC_BTN_COLOR_ZOOM_OUT))
	{
		if (pMsg->message == WM_LBUTTONDOWN)
		{
			SendZoomWide();
			return TRUE;
		}
		else if (pMsg->message == WM_LBUTTONUP)
		{
			SendZoomStop();
			return TRUE;
		}
	}
	else if (pMsg->hwnd == ::GetDlgItem(m_hWnd, IDC_BTN_COLOR_FOCUS_FAR))
	{
		if (pMsg->message == WM_LBUTTONDOWN)
		{
			SendFocusFar();
			return TRUE;
		}
		else if (pMsg->message == WM_LBUTTONUP)
		{
			SendFocusStop();
			return TRUE;
		}
	}
	else if (pMsg->hwnd == ::GetDlgItem(m_hWnd, IDC_BTN_COLOR_FOCUS_NEAR))
	{
		if (pMsg->message == WM_LBUTTONDOWN)
		{
			SendFocusNear();
			return TRUE;
		}
		else if (pMsg->message == WM_LBUTTONUP)
		{
			SendFocusStop();
			return TRUE;
		}
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CControl_ColorCam::OnBnClickedBtnColorAutofocus()
{
	SendAutoFocus();
}

void CControl_ColorCam::OnBnClickedBtnColorDaymode()
{
	SendDayMode();
}

void CControl_ColorCam::OnBnClickedBtnColorNightmode()
{
	SendNightMode();
}


void CControl_ColorCam::OnBnClickedBtnColorWiperUp()
{
	UpdateWindow();
	if (m_nWiperTime >= 15)
		return;
		
	m_nWiperTime++;

	CString str;
	str.Format(L"%d", m_nWiperTime);

	m_WiperDelayTime.SetWindowTextW(str);
}

void CControl_ColorCam::OnBnClickedBtnColorWiperDown()
{
	UpdateWindow();
	if (m_nWiperTime <= 1)
		return;

	m_nWiperTime--;

	CString str;
	str.Format(L"%d", m_nWiperTime);

	m_WiperDelayTime.SetWindowTextW(str);
}

void CControl_ColorCam::OnBnClickedBtnColorWiper()
{
	UpdateWindow();
	CString btn_wiper;
	m_btn_wiper.GetWindowTextW(btn_wiper);

	BYTE DT = (BYTE)m_nWiperTime;

	if (btn_wiper == L"구 동")
	{
		m_pComm->SendCCB(0, 0x3F, 1, DT);
		m_btn_wiper.SetWindowTextW(L"정 지");
	}
	else
	{
		m_pComm->SendCCB(0, 0x3F, 0, 0);
		m_btn_wiper.SetWindowTextW(L"구 동");
	}
}
