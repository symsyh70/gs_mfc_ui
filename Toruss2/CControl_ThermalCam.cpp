// CControl_ThermalCam.cpp: 구현 파일
//

#include "pch.h"
#include "Toruss2.h"
#include "afxdialogex.h"
#include "CControl_ThermalCam.h"


// CControl_ThermalCam 대화 상자

IMPLEMENT_DYNAMIC(CControl_ThermalCam, CDialogEx)

CControl_ThermalCam::CControl_ThermalCam(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_PANEL_THERMALCAM, pParent)
{

}

CControl_ThermalCam::~CControl_ThermalCam()
{
}

void CControl_ThermalCam::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CControl_ThermalCam, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_THERMAL_PALETTE_PREV, &CControl_ThermalCam::OnBnClickedBtnThermalPalettePrev)
	ON_BN_CLICKED(IDC_BTN_THERMAL_PALETTE_NEXT, &CControl_ThermalCam::OnBnClickedBtnThermalPaletteNext)
	ON_BN_CLICKED(IDC_BTN_THERMAL_PALETTE_WHITEGREY, &CControl_ThermalCam::OnBnClickedBtnThermalPaletteWhitegrey)
END_MESSAGE_MAP()


// CControl_ThermalCam 메시지 처리기

void CControl_ThermalCam::SetCommManager(CControl_Command* pComm)
{
	m_pComm = pComm;
}

BOOL CControl_ThermalCam::SendZoomTele()
{
	return m_pComm->SendCCB(0x00, 0x31, 0x00, 0x00);

}

BOOL CControl_ThermalCam::SendZoomWide()
{
	return m_pComm->SendCCB(0x00, 0x31, 0x01, 0x00);
}

BOOL CControl_ThermalCam::SendZoomStop()
{
	return m_pComm->SendCCB(0x00, 0x31, 0xFF, 0x00);
}

BOOL CControl_ThermalCam::SendFocusFar()
{
	return m_pComm->SendCCB(0x00, 0x31, 0x03, 0x00);

}

BOOL CControl_ThermalCam::SendFocusNear()
{
	return m_pComm->SendCCB(0x00, 0x31, 0x04, 0x00);
}

BOOL CControl_ThermalCam::SendFocusStop()
{
	return m_pComm->SendCCB(0x00, 0x31, 0x05, 0x00);
}

BOOL CControl_ThermalCam::SendAutoFocus()
{
	return m_pComm->SendCCB(0x00, 0x31, 0x02, 0x00);
}

BOOL CControl_ThermalCam::SendNextPalette()
{
	return m_pComm->SendCCB(0x00, 0x31, 0x0D, 0x00);
}

BOOL CControl_ThermalCam::SendPrevPalette()
{
	return m_pComm->SendCCB(0x00, 0x31, 0x0E, 0x00);
}

BOOL CControl_ThermalCam::SendGreyAndWhite()
{
	return m_pComm->SendCCB(0x00, 0x31, 0xF4, 0x00);
}
void CControl_ThermalCam::OnBnClickedBtnThermalPalettePrev()
{
	SendPrevPalette();
}

void CControl_ThermalCam::OnBnClickedBtnThermalPaletteNext()
{
	SendNextPalette();
}

void CControl_ThermalCam::OnBnClickedBtnThermalPaletteWhitegrey()
{
	SendGreyAndWhite();
}

