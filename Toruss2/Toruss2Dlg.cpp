
// Toruss2Dlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "Toruss2.h"
#include "Toruss2Dlg.h"
#include "afxdialogex.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/instance.hpp>

#include <gdiplus.h>
#include <cmath>                                                                                                                                                                        

#pragma comment(lib, "MOTWrapper.lib")

CToruss2Dlg* CToruss2Dlg::g_pFrameView = nullptr;

// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

const CToruss2Dlg::ZoomFovEntry CToruss2Dlg::g_zoomFovTable[] =
{
		{0,		18.40},
		{100,	15.08},
		{200,	12.02},
		{300,	9.02},
		{400,	6.19},
		{500,	3.67},
		{600,	1.81},
		{700,	1.02},
		{800,	0.65},
		{900,	0.45},
		{1000,	0.32},

		//{0,		39.79},
		//{100,	32.44},
		//{200,	26.15},
		//{300,	20.68},
		//{400,	15.82},
		//{500,	11.22},
		//{600,	8.53},
		//{700,	4.93},
		//{800,	2.9},
		//{900,	2.33},
		//{1000,	0.7958},
};

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

	// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	// 구현입니다.
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CToruss2Dlg 대화 상자



CString GetExePath()
{
	TCHAR path[MAX_PATH];
	GetModuleFileName(NULL, path, MAX_PATH);

	CString exePath = path;
	exePath = exePath.Left(exePath.ReverseFind('\\'));

	return exePath;
}


static double GetCpuUsagePercent()
{
	static ULONGLONG prevIdle = 0, prevKernel = 0, prevUser = 0;

	FILETIME idleFT, kernelFT, userFT;
	if (!GetSystemTimes(&idleFT, &kernelFT, &userFT))
		return 0.0;

	auto ToULL = [](const FILETIME& ft) -> ULONGLONG {
		return (ULONGLONG(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
		};

	ULONGLONG idle = ToULL(idleFT);
	ULONGLONG kernel = ToULL(kernelFT);
	ULONGLONG user = ToULL(userFT);

	ULONGLONG idleDelta = idle - prevIdle;
	ULONGLONG kernelDelta = kernel - prevKernel;
	ULONGLONG userDelta = user - prevUser;

	prevIdle = idle; prevKernel = kernel; prevUser = user;

	ULONGLONG total = kernelDelta + userDelta;
	if (total == 0) return 0.0;

	double busy = (double)(total - idleDelta) * 100.0 / (double)total;
	if (busy < 0) busy = 0;
	if (busy > 100) busy = 100;
	return busy;
}

static double GetRamUsagePercent()
{
	MEMORYSTATUSEX ms{};
	ms.dwLength = sizeof(ms);
	if (!GlobalMemoryStatusEx(&ms)) return 0.0;
	return (double)ms.dwMemoryLoad; // 0~100
}

static void GetRamMB(double& usedMB, double& totalMB)
{
	MEMORYSTATUSEX ms{};
	ms.dwLength = sizeof(ms);
	if (!GlobalMemoryStatusEx(&ms)) { usedMB = totalMB = 0; return; }

	totalMB = (double)ms.ullTotalPhys / (1024.0 * 1024.0);
	usedMB = (double)(ms.ullTotalPhys - ms.ullAvailPhys) / (1024.0 * 1024.0);
}

CToruss2Dlg::CToruss2Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_TORUSS2_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pLogo = NULL;

}

void CToruss2Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_LOGO, m_Logo);
	DDX_Control(pDX, IDC_STATIC_COLORCAM, m_Video0_ColorCam);
	DDX_Control(pDX, IDC_STATIC_THERMALCAM, m_Video1_ThermalCam);
	DDX_Control(pDX, IDC_STATIC_MINIMAP, m_Video2_MiniMap);
	DDX_Control(pDX, IDC_STATIC_PAGEAREA, m_PageArea);
	DDX_Control(pDX, IDC_STATIC_MAINVIEW, m_MainView);
}


BEGIN_MESSAGE_MAP(CToruss2Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_CONTROL_RANGE(
		BN_CLICKED,
		IDC_BUTTON_ConnectList,
		IDC_BUTTON_PANORAMA,
		&CToruss2Dlg::OnMenuButton
	)
	ON_WM_SIZE()
	ON_WM_DRAWITEM()
	ON_WM_TIMER()
	ON_WM_CTLCOLOR()
	ON_WM_HOTKEY()
	ON_MESSAGE(WM_VIDEO_RENDER, &CToruss2Dlg::OnVideoRender)
	ON_WM_LBUTTONDOWN()
//	ON_WM_KEYDOWN()
//	ON_WM_KEYUP()
END_MESSAGE_MAP()

CToruss2Dlg* g_pMainDlg = NULL;

// CToruss2Dlg 메시지 처리기

BOOL CToruss2Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.
	RegisterHotKey(m_hWnd, 1001, MOD_CONTROL | MOD_SHIFT, 'F');

	// TODO: 여기에 추가 초기화 작업을 추가합니다.

	CLogger::GetInstance().Init((GetExePath() + L"\\Log"));
	CString path = GetExePath() + L"\\Models\\best.onnx";
	std::string modelPath = CW2A(path);

	g_pMainDlg = this;
	CToruss2Dlg::g_pFrameView = this;
	if (m_DeepAI.initDetector(modelPath, 640))
	{
		OutputDebugString(L"Suc");
	}
	else
	{
		OutputDebugString(L"Out");
	}
	m_DeepAI.initTracker(2, 30, 2, 0.6, 0.6, 0.8);

	m_brStatBg.CreateSolidBrush(RGB(49, 49, 49));

	m_panorama_onoff = false;

	GetDlgItem(IDC_STATIC_COLORCAM)->ModifyStyle(0, SS_NOTIFY);
	GetDlgItem(IDC_STATIC_THERMALCAM)->ModifyStyle(0, SS_NOTIFY);
	GetDlgItem(IDC_STATIC_MINIMAP)->ModifyStyle(0, SS_NOTIFY);
	GetDlgItem(IDC_STATIC_MAINVIEW)->ModifyStyle(0, SS_NOTIFY);

	INIT_BTN(m_btn_ConnectList, IDC_BUTTON_ConnectList);
	INIT_BTN(m_btn_GearControl, IDC_BUTTON_GearControl);
	INIT_BTN(m_btn_VideoControl, IDC_BUTTON_VideoControl);
	INIT_BTN(m_btn_Panorama, IDC_BUTTON_PANORAMA);

	m_menuButtons = {
	&m_btn_ConnectList,
	&m_btn_GearControl,
	&m_btn_VideoControl,
	};

	m_btn_ConnectList.SetSelected(true);

	m_bInit = TRUE;
	SetWindowPos(&wndTop, 0, 0, 1920, 1080, SWP_NOMOVE);
	SetBackgroundColor(RGB(49, 49, 49));

	ModifyStyle(0, WS_CLIPSIBLINGS);

	CString logoPath = GetExePath() + L"\\img\\Logo.png";
	m_pLogo = Gdiplus::Image::FromFile(logoPath);

	GetDlgItem(IDC_STATIC_LOGO)->Invalidate();

	if (m_mongo.Connect("mongodb://127.0.0.1:27017"))
	{
		if (m_mongo.Ping())
		{
			CLogger::GetInstance().WriteLog(_T("SYSTEM"), _T("MongoDB Connect"));
		}
		else
		{
			CLogger::GetInstance().WriteLog(_T("SYSTEM"), _T("MongoDB Ping Faild"));
		}
	}
	else
	{
		CLogger::GetInstance().WriteLog(_T("SYSTEM"), _T("MongoDB Not Connected"));
	}

	m_pageConnect.SetMainDlg(this);

	m_VideoView.Initialize(this, &m_MainView, &m_Video0_ColorCam, &m_Video1_ThermalCam, &m_Video2_MiniMap);

	m_MainView.SetMainDlg(this);
	m_MainView.SetImageFolder(GetExePath() + L"\\img");

	InitPages();
	ShowPage(PAGE_TYPE::CONNECT);

	CLogger::GetInstance().WriteLog(_T("SYSTEM"), _T("UI Start"));

	m_VideoView.SetMainDlg(this);

	m_VideoView.LoadOverlayImages(GetExePath() + L"\\img");
	LoadCrosshairCalibration();

	CreatePanoramaPage();
	m_pPanorama->SetCommManager(&m_Command);
	m_pPanorama->SetVideoView(&m_VideoView);
	m_pPanorama->SetHFOV(39.79);
	m_pPanorama->SetOverlap(0.45);
	m_MainView.SetCommManager(&m_Command);
	m_ColorCam.SetCommManager(&m_Command);
	m_ThermalCam.SetCommManager(&m_Command);

	if (m_pPanorama && ::IsWindow(m_pPanorama->GetSafeHwnd()))
	{
		m_pPanorama->ShowWindow(SW_HIDE);
		m_pPanorama->SetWindowPos(
			nullptr,
			300, 60,
			1620, 1020,
			SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW);
	}
	m_panorama_onoff = false;

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void CToruss2Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CToruss2Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{

		CPaintDC dc(this);

		CRect rcClient;
		GetClientRect(&rcClient);

		CRect rcTitle = rcClient;

		rcTitle.bottom = 60;

		dc.FillSolidRect(rcTitle, RGB(34, 34, 34));

	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CToruss2Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CToruss2Dlg::OnMenuButton(UINT nID)
{
	switch (nID)
	{
	case IDC_BUTTON_PANORAMA:
		TogglePanoramaPage();
		return;
		break;
	case IDC_BUTTON_ConnectList:
		ShowPage(PAGE_TYPE::CONNECT);
		break;
	case IDC_BUTTON_GearControl:
		ShowPage(PAGE_TYPE::GEAR);
		break;
	case IDC_BUTTON_VideoControl:
		ShowPage(PAGE_TYPE::VIDEO);
		break;
	}

	for (auto btn : m_menuButtons)
		btn->SetSelected(btn->GetDlgCtrlID() == nID);
}

void Received_Data(COMMAND_DATA& pData)
{
	g_pMainDlg->ParseAndSendCommand(pData);
}
void CToruss2Dlg::ParseAndSendCommand(COMMAND_DATA& pData)
{
	if (pData.nLength < 2)
		return;

	BYTE* buf = pData.data;

	if (buf[0] != 0xFF)
		return;

	BYTE cmdID = buf[1];

	printf("[CMD] 0x%02X\n", cmdID);

	if (pData.nLength >= 12)
	{
		BYTE sum = 0;
		for (int i = 1; i <= 10; i++)
			sum += buf[i];

		sum &= 0xFF;

		if (buf[11] != sum)
		{
			printf("Checksum Error\n");
			return;
		}
	}

	switch (cmdID)
	{
	case 0x01: // Pan/Tilt
	{
		short nTmp0 = 0;

		double pan;
		double tilt;
		short Zoom;
		short Focus;

		memcpy(&nTmp0, &buf[2], 2);
		pan = nTmp0 / 100.0;

		memcpy(&nTmp0, &buf[4], 2);
		tilt = nTmp0 / 100.0;

		memcpy(&nTmp0, &buf[6], 2);
		Zoom = nTmp0;

		memcpy(&nTmp0, &buf[8], 2);
		Focus = nTmp0;

		m_fPan = pan;
		m_fTilt = tilt;
		m_nFocusValue = Focus;

		UpdateFovByZoom(Zoom);

		double dPan = pan;
		double dTilt = tilt;

		OnCCBTelemetry(dPan, dTilt, Zoom, Focus, 0);
		break;
	}
	case 0x02: // Longitude
	{
		unsigned short degree = 0;
		unsigned short minute = 0;
		float second = 0.0f;
		char azimuth = 0;

		memcpy(&degree, &buf[2], 2);
		memcpy(&minute, &buf[4], 2);
		memcpy(&second, &buf[6], 4);
		azimuth = buf[10];

		double lon =
			(double)degree +
			(double)minute / 60.0 +
			(double)second / 3600.0;

		if (azimuth == 'W')
			lon = -lon;

		m_fUnitLon = lon;

		// ⭐ minimap 반영
		m_VideoView.SetMiniMapCameraGeo(m_fUnitLat, m_fUnitLon);

		printf("[GPS] Lon: %.8f\n", lon);
		break;
	}

	case 0x03: // Latitude
	{
		unsigned short degree = 0;
		unsigned short minute = 0;
		float second = 0.0f;
		char azimuth = 0;

		memcpy(&degree, &buf[2], 2);
		memcpy(&minute, &buf[4], 2);
		memcpy(&second, &buf[6], 4);
		azimuth = buf[10];

		double lat =
			(double)degree +
			(double)minute / 60.0 +
			(double)second / 3600.0;

		if (azimuth == 'S')
			lat = -lat;

		m_fUnitLat = lat;

		// ⭐ minimap 반영
		m_VideoView.SetMiniMapCameraGeo(m_fUnitLat, m_fUnitLon);

		printf("[GPS] Lat: %.8f\n", lat);
		break;
	}
	case 0x07:
		short nTmp0 = 0;

		short Zoom;
		short Focus;

		memcpy(&nTmp0, &buf[2], 2);
		Zoom = nTmp0;

		memcpy(&nTmp0, &buf[4], 2);
		Focus = nTmp0;

		m_nThermalZoom = Zoom;
		m_nThermalFocus = Focus;

		if (m_VideoView.GetMainSource() == CVideoView::VIEW_SOURCE::THERMAL)
			m_VideoView.MarkComposeDirty();
	}
}

void CToruss2Dlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (m_bInit)
	{
		CRect rc, rcLogo, rcClock, rcDeviceM, rcDeviceC, rcVideoC, rcVideo0,
			rcVideo1, rcVideo2, rcPageArea, rcMainView, rcPanorama,
			rcPanDeg, rcTiltDeg;
		GetClientRect(rc);

		rcVideo0.left = 300;
		rcVideo0.top = rcVideo0.bottom + 60;
		rcVideo0.right = rcVideo0.left + 480;
		rcVideo0.bottom = rcVideo0.top + 340;
		m_Video0_ColorCam.MoveWindow(rcVideo0);

		rcVideo1.left = 300;
		rcVideo1.top = rcVideo0.bottom;
		rcVideo1.right = rcVideo1.left + 480;
		rcVideo1.bottom = rcVideo1.top + 340;
		m_Video1_ThermalCam.MoveWindow(rcVideo1);

		rcVideo2.left = 300;
		rcVideo2.top = rcVideo1.bottom;
		rcVideo2.right = rcVideo2.left + 480;
		rcVideo2.bottom = rc.bottom;
		m_Video2_MiniMap.MoveWindow(rcVideo2);

		rcMainView.left = rcVideo0.right;
		rcMainView.top = rcVideo0.top;
		rcMainView.right = rc.right;
		rcMainView.bottom = rc.bottom;
		m_MainView.MoveWindow(rcMainView);

		rcLogo.left = 100;
		rcLogo.top = rcLogo.bottom + 5;
		rcLogo.right = rcLogo.left + 230;
		rcLogo.bottom = rcLogo.top + 50;
		m_Logo.MoveWindow(rcLogo);

		rcDeviceM.left = 0;
		rcDeviceM.top = rcDeviceM.bottom + 60;
		rcDeviceM.right = rcDeviceM.left + 100;
		rcDeviceM.bottom = rcDeviceM.top + 57;
		m_btn_ConnectList.MoveWindow(rcDeviceM);

		rcDeviceC.left = rcDeviceM.left + 100;
		rcDeviceC.top = rcDeviceC.bottom + 60;
		rcDeviceC.right = rcDeviceC.left + 100;
		rcDeviceC.bottom = rcDeviceC.top + 57;
		m_btn_GearControl.MoveWindow(rcDeviceC);

		rcVideoC.left = rcDeviceC.left + 100;
		rcVideoC.top = rcVideoC.bottom + 60;
		rcVideoC.right = rcVideoC.left + 100;
		rcVideoC.bottom = rcVideoC.top + 57;
		m_btn_VideoControl.MoveWindow(rcVideoC);

		rcPageArea.left = rcDeviceM.left;
		rcPageArea.top = rcDeviceM.bottom;
		rcPageArea.right = rcDeviceM.left + 300;
		rcPageArea.bottom = rc.bottom;
		m_PageArea.MoveWindow(rcPageArea);

		rcPanorama.left = rcLogo.right + 120;
		rcPanorama.top = rcLogo.top + 5;
		rcPanorama.right = rcPanorama.left + 140;
		rcPanorama.bottom = rcPanorama.top + 40;
		m_btn_Panorama.MoveWindow(rcPanorama);

		if (m_pPanorama && ::IsWindow(m_pPanorama->GetSafeHwnd()))
		{
			UpdatePanoramaLayout(m_panorama_onoff ? TRUE : FALSE);
		}
	}

}
void CToruss2Dlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (nIDCtl == IDC_STATIC_LOGO)
	{
		CDC dc;
		dc.Attach(lpDrawItemStruct->hDC);

		CRect rc = lpDrawItemStruct->rcItem;
		dc.FillSolidRect(&rc, RGB(34, 34, 34)); // 타이틀 배경색으로 직접 칠함

		if (m_pLogo)
		{
			Gdiplus::Graphics g(dc);
			g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

			const double iw = (double)m_pLogo->GetWidth();
			const double ih = (double)m_pLogo->GetHeight();
			const double cw = (double)rc.Width();
			const double ch = (double)rc.Height();

			double s = std::min(cw / iw, ch / ih);
			int w = (int)(iw * s);
			int h = (int)(ih * s);

			int x = rc.left + (rc.Width() - w) / 2;
			int y = rc.top + (rc.Height() - h) / 2;

			g.DrawImage(m_pLogo, x, y, w, h);
		}

		dc.Detach();
		return;
	}


	CDialogEx::OnDrawItem(nIDCtl, lpDrawItemStruct);
}

void CToruss2Dlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1)
	{
		double cpu = GetCpuUsagePercent();
		double ramP = GetRamUsagePercent();
		double usedMB, totalMB; GetRamMB(usedMB, totalMB);

		CString s;
		s.Format(L"CPU: %.1f%%      RAM: %.0f%% (%.0f / %.0f MB)", cpu, ramP, usedMB, totalMB);

		SetDlgItemText(IDC_STATIC_STAT, s);
	}
	if (nIDEvent == 2)
	{
		if (m_CamCtrlClient.IsConnected())
		{
			SendZoomPositionInquiry();
			SendFocusPositionInquiry();
		}
	}


	CDialogEx::OnTimer(nIDEvent);
}

HBRUSH CToruss2Dlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

	if (nCtlColor == CTLCOLOR_STATIC && pWnd->GetDlgCtrlID() == IDC_STATIC_STAT)
	{
		pDC->SetBkMode(OPAQUE);                 // ✅ 배경 칠하기
		pDC->SetBkColor(RGB(45, 45, 45));         // 배경색
		pDC->SetTextColor(RGB(255, 255, 255));    // 글자색
		return (HBRUSH)m_brStatBg.GetSafeHandle(); // ✅ 배경 지워짐(겹침 제거)
	}
	return hbr;
}

void CToruss2Dlg::InitPages()
{
	CRect rc;
	GetDlgItem(IDC_STATIC_PAGEAREA)->GetWindowRect(&rc);
	ScreenToClient(&rc);

	m_pageConnect.Create(IDD_DIALOG_CONNECT, this);
	m_pageGear.Create(IDD_DIALOG_GEAR, this);
	m_pageVideo.Create(IDD_DIALOG_VIDEO, this);

	m_pageConnect.MoveWindow(rc);
	m_pageGear.MoveWindow(rc);
	m_pageVideo.MoveWindow(rc);

	m_pageConnect.SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	m_pageGear.SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	m_pageVideo.SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	m_pageConnect.ShowWindow(SW_HIDE);
	m_pageGear.ShowWindow(SW_HIDE);
	m_pageVideo.ShowWindow(SW_HIDE);

	m_ColorCam.Create(IDD_PANEL_COLORCAM, &m_pageVideo);
	m_ThermalCam.Create(IDD_PANEL_THERMALCAM, &m_pageVideo);

	m_ColorCam.ShowWindow(SW_HIDE);
	m_ThermalCam.ShowWindow(SW_HIDE);

	m_pageVideo.SetSubDialogs(&m_ColorCam, &m_ThermalCam);
}

void CToruss2Dlg::ShowPage(PAGE_TYPE type)
{
	m_pageConnect.ShowWindow(SW_HIDE);
	m_pageGear.ShowWindow(SW_HIDE);
	m_pageVideo.ShowWindow(SW_HIDE);

	CWnd* pShow = nullptr;

	switch (type)
	{
	case PAGE_TYPE::CONNECT:
		pShow = &m_pageConnect;
		break;

	case PAGE_TYPE::GEAR:
		pShow = &m_pageGear;
		break;

	case PAGE_TYPE::VIDEO:
		pShow = &m_pageVideo;
		break;
	}

	if (pShow && ::IsWindow(pShow->GetSafeHwnd()))
	{
		pShow->SetWindowPos(
			&wndTop,
			0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
		pShow->Invalidate();
		pShow->UpdateWindow();
	}

	m_currentPage = type;
}

void CToruss2Dlg::OpenSelectedDevice(const DeviceProfile& profile)
{
	bool expected = false;
	if (!m_isOpening.compare_exchange_strong(expected, true))
	{
		OutputDebugString(L"[OpenSelectedDevice] skip - already opening\r\n");
		return;
	}

	CString colorRtsp = CRtspBuilder::BuildColorRtsp(profile);
	CString thermalRtsp = CRtspBuilder::BuildThermalRtsp(profile);

	CLogger::GetInstance().WriteLog(_T("Color VIDEO RTSP"), colorRtsp.GetString());
	CLogger::GetInstance().WriteLog(_T("Thermal VIDEO RTSP"), thermalRtsp.GetString());

	m_VideoView.CloseStreams();

	CString miniMapPath = GetExePath() + L"\\MiniMap\\Test.png";
	m_VideoView.LoadMiniMapImage(miniMapPath);

	if (!colorRtsp.IsEmpty())
	{
		if (!m_VideoView.OpenColorStream(colorRtsp))
			OutputDebugString(L"[OpenSelectedDevice] Color open fail\r\n");
	}

	if (!thermalRtsp.IsEmpty())
	{
		if (!m_VideoView.OpenThermalStream(thermalRtsp))
			OutputDebugString(L"[OpenSelectedDevice] Thermal open fail\r\n");
	}

	m_isOpening = false;
}

BOOL CToruss2Dlg::ConnectCCB(const DeviceProfile& profile)
{
	if (!m_Command.ConnectCCB(profile.ccbip, _ttoi(profile.ccbport)))
		return FALSE;

	m_Command.SetRecvCallback(Received_Data, this);

	return TRUE;
}

void CToruss2Dlg::OnCCBTelemetry(double panDeg, double tiltDeg, int zoomPos, int focusPos, int powerStatus)
{
	m_fPan = panDeg;
	m_fTilt = tiltDeg;
	m_nZoom = zoomPos;
	m_nFocus = focusPos;
	m_nPower = powerStatus;

	// 현재 zoom 기준 HFOV 유지
	m_fHFOV = GetHFovFromZoom(m_nZoom);
	m_nZoomValue = m_nZoom;

	// Panorama 있으면 여기서 반영
	if (m_pPanorama && ::IsWindow(m_pPanorama->GetSafeHwnd()))
	{
		double hfov = m_fHFOV;
		double vfov = GetVFovFromTilt(m_fTilt);
		m_pPanorama->SetCurrentView(m_fPan, m_fTilt, hfov, vfov);
	}

	UpdateJogTracking();
}

BOOL CToruss2Dlg::PreTranslateMessage(MSG* pMsg)
{
	HWND hMiniMap = GetDlgItem(IDC_STATIC_MINIMAP)->GetSafeHwnd();

	if (pMsg->message == WM_KEYDOWN)
	{
		KEY_TARGET_VIEW target = GetCurrentKeyTargetView();
		if (pMsg->wParam == 'Z')
		{
			if (!m_bSpeedKeyDownZ)
			{
				m_bSpeedKeyDownZ = true;
				m_bShowManualPTZSpeed = true;
				m_VideoView.MarkComposeDirty();
			}

			ChangeManualPTZSpeed(-1);
			return TRUE;
		}
		else if (pMsg->wParam == 'C')
		{
			if (!m_bSpeedKeyDownC)
			{
				m_bSpeedKeyDownC = true;
				m_bShowManualPTZSpeed = true;
				m_VideoView.MarkComposeDirty();
			}

			ChangeManualPTZSpeed(+1);
			return TRUE;
		}
		else if (pMsg->wParam == 'W')
		{

			if (!m_bZoomKeyDownW)
				m_bZoomKeyDownW = true;

			if (target == KEY_TARGET_VIEW::COLOR)
				m_ColorCam.SendZoomTele();
			else if (target == KEY_TARGET_VIEW::THERMAL)
				m_ThermalCam.SendZoomTele();
			return TRUE;
		}
		else if (pMsg->wParam == 'S')
		{
			if (!m_bZoomKeyDownS)
				m_bZoomKeyDownS = true;

			if (target == KEY_TARGET_VIEW::COLOR)
				m_ColorCam.SendZoomWide();
			else if (target == KEY_TARGET_VIEW::THERMAL)
				m_ThermalCam.SendZoomWide();
			return TRUE;
		}
		else if (pMsg->wParam == 'A')
		{
			if (!m_bFocusKeyDownA)
				m_bFocusKeyDownA = true;

			if (target == KEY_TARGET_VIEW::COLOR)
				m_ColorCam.SendFocusNear();
			else if (target == KEY_TARGET_VIEW::THERMAL)
				m_ThermalCam.SendFocusNear();
			return TRUE;
		}
		else if (pMsg->wParam == 'D')
		{
			if (!m_bFocusKeyDownD)
				m_bFocusKeyDownD = true;

			if (target == KEY_TARGET_VIEW::COLOR)
				m_ColorCam.SendFocusFar();
			else if (target == KEY_TARGET_VIEW::THERMAL)
				m_ThermalCam.SendFocusFar();
			return TRUE;
		}
		else if (pMsg->wParam == VK_LEFT)
		{
			m_bMoveKeyDownLeft = true;
			UpdateKeyboardMoveCommand();
			return TRUE;
		}
		else if (pMsg->wParam == VK_UP)
		{
			m_bMoveKeyDownUp = true;
			UpdateKeyboardMoveCommand();
			return TRUE;
		}
		else if (pMsg->wParam == VK_RIGHT)
		{
			m_bMoveKeyDownRight = true;
			UpdateKeyboardMoveCommand();
			return TRUE;
		}
		else if (pMsg->wParam == VK_DOWN)
		{
			m_bMoveKeyDownDown = true;
			UpdateKeyboardMoveCommand();
			return TRUE;
		}
	}
	else if (pMsg->message == WM_KEYUP)
	{
		KEY_TARGET_VIEW target = GetCurrentKeyTargetView();
		if (pMsg->wParam == 'Z')
		{
			m_bSpeedKeyDownZ = false;

			if (!m_bSpeedKeyDownC)
			{
				m_bShowManualPTZSpeed = false;
				m_VideoView.MarkComposeDirty();
			}
			return TRUE;
		}
		else if (pMsg->wParam == 'C')
		{
			m_bSpeedKeyDownC = false;

			if (!m_bSpeedKeyDownZ)
			{
				m_bShowManualPTZSpeed = false;
				m_VideoView.MarkComposeDirty();
			}
			return TRUE;
		}
		else if (pMsg->wParam == 'W')
		{
			m_bZoomKeyDownW = false;
			if (target == KEY_TARGET_VIEW::COLOR)
				m_ColorCam.SendZoomStop();
			else if (target == KEY_TARGET_VIEW::THERMAL)
				m_ThermalCam.SendZoomStop();
			return TRUE;
		}
		else if (pMsg->wParam == 'S')
		{
			m_bZoomKeyDownS = false;
			if (target == KEY_TARGET_VIEW::COLOR)
				m_ColorCam.SendZoomStop();
			else if (target == KEY_TARGET_VIEW::THERMAL)
				m_ThermalCam.SendZoomStop();
			return TRUE;
		}
		else if (pMsg->wParam == 'A')
		{
			m_bFocusKeyDownA = false;
			if (target == KEY_TARGET_VIEW::COLOR)
				m_ColorCam.SendFocusStop();
			else if (target == KEY_TARGET_VIEW::THERMAL)
				m_ThermalCam.SendFocusStop();
			return TRUE;
		}
		else if (pMsg->wParam == 'D')
		{
			m_bFocusKeyDownD = false;
			if (target == KEY_TARGET_VIEW::COLOR)
				m_ColorCam.SendFocusStop();
			else if (target == KEY_TARGET_VIEW::THERMAL)
				m_ThermalCam.SendFocusStop();
			return TRUE;
		}
		else if (pMsg->wParam == VK_LEFT)
		{
			m_bMoveKeyDownLeft = false;
			UpdateKeyboardMoveCommand();
			return TRUE;
		}
		else if (pMsg->wParam == VK_UP)
		{
			m_bMoveKeyDownUp = false;
			UpdateKeyboardMoveCommand();
			return TRUE;
		}
		else if (pMsg->wParam == VK_RIGHT)
		{
			m_bMoveKeyDownRight = false;
			UpdateKeyboardMoveCommand();
			return TRUE;
		}
		else if (pMsg->wParam == VK_DOWN)
		{
			m_bMoveKeyDownDown = false;
			UpdateKeyboardMoveCommand();
			return TRUE;
		}
	}

	if (hMiniMap && pMsg->hwnd == hMiniMap)
	{
		CRect rcMini;
		GetDlgItem(IDC_STATIC_MINIMAP)->GetClientRect(&rcMini);

		switch (pMsg->message)
		{
		case WM_LBUTTONDOWN:
		{
			m_bMiniMapDragging = true;
			m_ptMiniMapLast.x = GET_X_LPARAM(pMsg->lParam);
			m_ptMiniMapLast.y = GET_Y_LPARAM(pMsg->lParam);
			::SetCapture(hMiniMap);
			return TRUE;
		}

		case WM_MOUSEMOVE:
		{
			if (m_bMiniMapDragging)
			{
				CPoint pt;
				pt.x = GET_X_LPARAM(pMsg->lParam);
				pt.y = GET_Y_LPARAM(pMsg->lParam);

				int dx = pt.x - m_ptMiniMapLast.x;
				int dy = pt.y - m_ptMiniMapLast.y;

				m_VideoView.PanMiniMapByViewDelta(dx, dy, rcMini.Width(), rcMini.Height());

				m_ptMiniMapLast = pt;
				return TRUE;
			}
			break;
		}

		case WM_LBUTTONUP:
		{
			CPoint pt;
			pt.x = GET_X_LPARAM(pMsg->lParam);
			pt.y = GET_Y_LPARAM(pMsg->lParam);

			if (m_bMiniMapDragging)
			{
				m_bMiniMapDragging = false;
				if (::GetCapture() == hMiniMap)
					::ReleaseCapture();
				return TRUE;
			}

			double lat = 0.0;
			double lon = 0.0;
			if (m_VideoView.MiniMapViewPointToLatLon(pt.x, pt.y, rcMini.Width(), rcMini.Height(), lat, lon))
			{
				CString msg;
				msg.Format(L"Clicked LAT: %.6f, LON: %.6f", lat, lon);
				OutputDebugString(msg + L"\r\n");

				AfxMessageBox(msg);
			}
			return TRUE;
		}

		case WM_MOUSEWHEEL:
		{
			CPoint screenPt;
			screenPt.x = GET_X_LPARAM(pMsg->lParam);
			screenPt.y = GET_Y_LPARAM(pMsg->lParam);

			CPoint clientPt = screenPt;
			::ScreenToClient(hMiniMap, &clientPt);

			short zDelta = GET_WHEEL_DELTA_WPARAM(pMsg->wParam);

			m_VideoView.ZoomMiniMapAtPoint(
				clientPt.x, clientPt.y,
				rcMini.Width(), rcMini.Height(),
				zDelta);

			return TRUE;
		}
		}
	}
	HWND hMain = GetDlgItem(IDC_STATIC_MAINVIEW)->GetSafeHwnd();
	if (hMain && pMsg->hwnd == hMain && pMsg->message == WM_LBUTTONDOWN)
	{
		CRect rcMain;
		GetDlgItem(IDC_STATIC_MAINVIEW)->GetClientRect(&rcMain);

		CPoint pt;
		pt.x = GET_X_LPARAM(pMsg->lParam);
		pt.y = GET_Y_LPARAM(pMsg->lParam);

		if (m_bCrosshairCalibrationMode)
		{
			AddCrosshairCalibrationPoint(pt, rcMain.Width(), rcMain.Height());
			m_VideoView.MarkComposeDirty();
			return TRUE;
		}

		if ((::GetKeyState(VK_CONTROL) & 0x8000) != 0)
		{
			// Ctrl + Click = 그 위치로 직접 이동
			m_VideoView.StopClickDrive();/*
			m_VideoView.MovePTZToViewPoint(pt);*/
			return TRUE;
		}
		else
		{
			// 일반 클릭 = bbox 선택/해제 추적
			if (m_VideoView.StartClickDriveFromBBox(pt))
				return TRUE;
		}
	}
	if (pMsg->message == WM_RBUTTONDBLCLK)
	{
		HWND hWnd = pMsg->hwnd;

		HWND hColor = GetDlgItem(IDC_STATIC_COLORCAM)->GetSafeHwnd();
		HWND hThermal = GetDlgItem(IDC_STATIC_THERMALCAM)->GetSafeHwnd();
		HWND hMiniMap2 = GetDlgItem(IDC_STATIC_MINIMAP)->GetSafeHwnd();
		HWND hMain = GetDlgItem(IDC_STATIC_MAINVIEW)->GetSafeHwnd();

		if (hWnd == hMain)
		{
			ToggleMainFullscreen();
			return TRUE;
		}

		if (!m_bMainFullscreen)
		{
			if (hWnd == hColor)
			{
				m_VideoView.SwapMainWith(CVideoView::VIEW_SOURCE::COLOR);
				return TRUE;
			}
			else if (hWnd == hThermal)
			{
				m_VideoView.SwapMainWith(CVideoView::VIEW_SOURCE::THERMAL);
				return TRUE;
			}
			else if (hWnd == hMiniMap2)
			{
				m_VideoView.SwapMainWith(CVideoView::VIEW_SOURCE::MINIMAP);
				return TRUE;
			}
		}
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CToruss2Dlg::OnHotKey(UINT nHotKeyId, UINT nKey1, UINT nKey2)
{
	UNREFERENCED_PARAMETER(nKey1);
	UNREFERENCED_PARAMETER(nKey2);

		switch (nHotKeyId)
		{
		case 1001: // Z
			SetCrosshairCalibrationMode(!IsCrosshairCalibrationMode());
			m_VideoView.MarkComposeDirty();
			return;
			break;
		default:
			break;
		
	}

	CDialogEx::OnHotKey(nHotKeyId, nKey1, nKey2);
}

BOOL CToruss2Dlg::CreatePanoramaPage()
{
	if (m_pPanorama == nullptr)
		m_pPanorama = new CControl_Panorama;

	if (!::IsWindow(m_pPanorama->GetSafeHwnd()))
	{
		if (!m_pPanorama->Create(IDD_DIALOG_PANORAMA, this))
		{
			delete m_pPanorama;
			m_pPanorama = nullptr;
			return FALSE;
		}
	}

	return TRUE;
}

void CToruss2Dlg::TogglePanoramaPage()
{
	if (!CreatePanoramaPage())
		return;

	if (!::IsWindow(m_pPanorama->GetSafeHwnd()))
		return;

	if (m_panorama_onoff)
	{
		UpdatePanoramaLayout(FALSE);
		m_panorama_onoff = false;
	}
	else
	{
		m_pPanorama->ClearPreviewFrameOnly();
		UpdatePanoramaLayout(TRUE);
		m_panorama_onoff = true;
	}
}

void CToruss2Dlg::ToggleMainFullscreen()
{
	CWnd* pColor = GetDlgItem(IDC_STATIC_COLORCAM);
	CWnd* pThermal = GetDlgItem(IDC_STATIC_THERMALCAM);
	CWnd* pMiniMap = GetDlgItem(IDC_STATIC_MINIMAP);
	CWnd* pMain = GetDlgItem(IDC_STATIC_MAINVIEW);

	if (!pMain)
		return;

	if (!m_bMainFullscreen)
	{
		if (pColor)   pColor->GetWindowRect(&m_rcColor);
		if (pThermal) pThermal->GetWindowRect(&m_rcThermal);
		if (pMiniMap) pMiniMap->GetWindowRect(&m_rcMiniMap);
		pMain->GetWindowRect(&m_rcMain);

		ScreenToClient(&m_rcColor);
		ScreenToClient(&m_rcThermal);
		ScreenToClient(&m_rcMiniMap);
		ScreenToClient(&m_rcMain);

		if (pColor)   pColor->ShowWindow(SW_HIDE);
		if (pThermal) pThermal->ShowWindow(SW_HIDE);
		if (pMiniMap) pMiniMap->ShowWindow(SW_HIDE);

		CRect rcClient;
		GetClientRect(&rcClient);

		CRect rcBtn;
		m_btn_ConnectList.GetWindowRect(&rcBtn);
		ScreenToClient(&rcBtn);

		CRect rcVideoArea;
		rcVideoArea.left = 300;
		rcVideoArea.top = 60;
		rcVideoArea.right = rcClient.right;
		rcVideoArea.bottom = rcClient.bottom;

		pMain->SetWindowPos(nullptr,
			rcVideoArea.left, rcVideoArea.top,
			rcVideoArea.Width(), rcVideoArea.Height(),
			SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

		m_bMainFullscreen = true;
	}
	else
	{
		if (pColor)
		{
			pColor->SetWindowPos(nullptr,
				m_rcColor.left, m_rcColor.top,
				m_rcColor.Width(), m_rcColor.Height(),
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
			pColor->ShowWindow(SW_SHOW);
		}

		if (pThermal)
		{
			pThermal->SetWindowPos(nullptr,
				m_rcThermal.left, m_rcThermal.top,
				m_rcThermal.Width(), m_rcThermal.Height(),
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
			pThermal->ShowWindow(SW_SHOW);
		}

		if (pMiniMap)
		{
			pMiniMap->SetWindowPos(nullptr,
				m_rcMiniMap.left, m_rcMiniMap.top,
				m_rcMiniMap.Width(), m_rcMiniMap.Height(),
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
			pMiniMap->ShowWindow(SW_SHOW);
		}

		pMain->SetWindowPos(nullptr,
			m_rcMain.left, m_rcMain.top,
			m_rcMain.Width(), m_rcMain.Height(),
			SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

		m_bMainFullscreen = false;
	}

	m_VideoView.SetMainFullscreen(m_bMainFullscreen);
	RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

LRESULT CToruss2Dlg::OnVideoRender(WPARAM wParam, LPARAM lParam)
{

	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);


	bool expected = false;
	if (!m_bRendering.compare_exchange_strong(expected, true))
		return 0;

	m_VideoView.OnRenderMessage();

	m_bRendering = false;
	return 0;
}

double CToruss2Dlg::GetHFovFromZoom(int zoomValue)
{
	const int count = sizeof(g_zoomFovTable) / sizeof(g_zoomFovTable[0]);

	if (zoomValue <= g_zoomFovTable[0].zoom)
		return g_zoomFovTable[0].hfov;

	if (zoomValue >= g_zoomFovTable[count - 1].zoom)
		return g_zoomFovTable[count - 1].hfov;

	for (int i = 0; i < count - 1; ++i)
	{
		const auto& a = g_zoomFovTable[i];
		const auto& b = g_zoomFovTable[i + 1];

		if (zoomValue >= a.zoom && zoomValue <= b.zoom)
		{
			double t = double(zoomValue - a.zoom) / double(b.zoom - a.zoom);
			return a.hfov + (b.hfov - a.hfov) * t;
		}
	}

	return g_zoomFovTable[count - 1].hfov;
}

double CToruss2Dlg::GetVFovFromTilt(double tiltDeg) const
{
	double minTilt = m_tiltMinDegForVFov;
	double maxTilt = m_tiltMaxDegForVFov;

	if (minTilt > maxTilt)
		std::swap(minTilt, maxTilt);

	if (tiltDeg <= minTilt)
		return m_vFovAtTiltMin;

	if (tiltDeg >= maxTilt)
		return m_vFovAtTiltMax;

	const double t = (tiltDeg - minTilt) / (maxTilt - minTilt);
	return m_vFovAtTiltMin + (m_vFovAtTiltMax - m_vFovAtTiltMin) * t;
}

double CToruss2Dlg::GetVFovFromZoom(int zoomValue, int frameW, int frameH)
{
	const double hfovDeg = GetHFovFromZoom(zoomValue);

	if (frameW <= 0 || frameH <= 0)
	{
		// fallback
		frameW = 16;
		frameH = 9;
	}

	const double hfovRad = hfovDeg * 3.14159265358979323846 / 180.0;
	const double vfovRad =
		2.0 * atan(tan(hfovRad * 0.5) * (static_cast<double>(frameH) / static_cast<double>(frameW)));

	double vfovDeg = vfovRad * 180.0 / 3.14159265358979323846;

	return vfovDeg;
}

void CToruss2Dlg::UpdateFovByZoom(int zoomValue)
{
	m_nZoomValue = zoomValue;
	m_fHFOV = GetHFovFromZoom(zoomValue);
}

double CToruss2Dlg::GetMagnifyingPower() const
{
	const double maxHFov = g_zoomFovTable[0].hfov; // zoom 0일 때 최대 화각
	const double curHFov = (m_fHFOV <= 0.0001) ? 0.0001 : m_fHFOV;
	return maxHFov / curHFov;
}

static double WrapPanDeg(double deg)
{
	while (deg > 180.0) deg -= 360.0;
	while (deg < -180.0) deg += 360.0;
	return deg;
}

bool CToruss2Dlg::MoveCameraToPanTilt(double panDeg, double tiltDeg, int panSpeed, int tiltSpeed)
{
	auto clampSpeed63 = [](int v) -> int
		{
			if (v < 0) return 0;
			if (v > 63) return 63;
			return v;
		};

	if (panSpeed < 0)  panSpeed = 20;
	if (tiltSpeed < 0) tiltSpeed = 20;

	m_targetJogPan = panDeg;
	m_targetJogTilt = tiltDeg;
	m_targetJogPanSpeed = clampSpeed63(panSpeed);
	m_targetJogTiltSpeed = clampSpeed63(tiltSpeed);
	m_bJogTracking = true;

	CString dbg;
	dbg.Format(
		L"[MoveCameraToPanTilt:JOG] curPan=%.3f curTilt=%.3f targetPan=%.3f targetTilt=%.3f panSpd=%d tiltSpd=%d\r\n",
		m_fPan, m_fTilt, m_targetJogPan, m_targetJogTilt, m_targetJogPanSpeed, m_targetJogTiltSpeed);
	OutputDebugString(dbg);

	UpdateJogTracking();
	return true;
}

CString CToruss2Dlg::GetCalibrationIniPath() const
{
	return GetExePath() + L"\Config\CrosshairCalibration.ini";

}

void CToruss2Dlg::LoadCrosshairCalibration()
{
	const CString iniPath = GetCalibrationIniPath();
	const int offsetX = GetPrivateProfileInt(L"Crosshair", L"OffsetX", 0, iniPath);
	const int offsetY = GetPrivateProfileInt(L"Crosshair", L"OffsetY", 0, iniPath);
	SetCrosshairCalibrationOffset(offsetX, offsetY);
	ResetCrosshairCalibrationPoints();
	m_bCrosshairCalibrationLineDraw = false;
}

void CToruss2Dlg::SaveCrosshairCalibration() const
{
	const CString iniPath = GetCalibrationIniPath();

	::CreateDirectory(GetExePath() + L"\Config", nullptr);

	CString value;
	value.Format(L"%d", m_ptCrosshairOffset.x);
	WritePrivateProfileString(L"Crosshair", L"OffsetX", value, iniPath);

	value.Format(L"%d", m_ptCrosshairOffset.y);
	WritePrivateProfileString(L"Crosshair", L"OffsetY", value, iniPath);
}

void CToruss2Dlg::SetCrosshairCalibrationOffset(int offsetX, int offsetY)
{
	m_ptCrosshairOffset.x = offsetX;
	m_ptCrosshairOffset.y = offsetY;
}

void CToruss2Dlg::ResetCrosshairCalibrationPoints()
{
	m_nCrosshairCalibrationPointCount = 0;
	for (int i = 0; i < 4; ++i)
	{
		m_crosshairCalibrationPoints[i] = CPoint(-3, -3);
		m_crosshairCalibrationLinePoints[i] = CPoint(-3, -3);
	}
}

int CToruss2Dlg::GetCrosshairCalibrationPointCount() const
{
	return m_nCrosshairCalibrationPointCount;
}

CPoint CToruss2Dlg::GetCrosshairCalibrationPoint(int index) const
{
	if (index < 0 || index >= 4)
		return CPoint(-3, -3);
	return m_crosshairCalibrationPoints[index];
}

bool CToruss2Dlg::HasCrosshairCalibrationLines() const
{
	return m_bCrosshairCalibrationLineDraw;
}

void CToruss2Dlg::GetCrosshairCalibrationLine(int lineIndex, CPoint& outStart, CPoint& outEnd) const
{
	outStart = CPoint(-3, -3);
	outEnd = CPoint(-3, -3);
	if (lineIndex < 0 || lineIndex > 1)
		return;

	const int base = lineIndex * 2;
	outStart = m_crosshairCalibrationLinePoints[base];
	outEnd = m_crosshairCalibrationLinePoints[base + 1];
}

bool CToruss2Dlg::AddCrosshairCalibrationPoint(const CPoint& clickPt, int viewW, int viewH)
{
	if (!m_bCrosshairCalibrationMode)
		return false;
	if (viewW <= 0 || viewH <= 0)
		return false;
	if (m_nCrosshairCalibrationPointCount < 0 || m_nCrosshairCalibrationPointCount > 3)
		m_nCrosshairCalibrationPointCount = 0;

	m_crosshairCalibrationPoints[m_nCrosshairCalibrationPointCount] = clickPt;
	++m_nCrosshairCalibrationPointCount;

	if (m_nCrosshairCalibrationPointCount < 4)
		return true;

	const auto& p0 = m_crosshairCalibrationPoints[0];
	const auto& p1 = m_crosshairCalibrationPoints[1];
	const auto& p2 = m_crosshairCalibrationPoints[2];
	const auto& p3 = m_crosshairCalibrationPoints[3];

	double centerX = static_cast<double>(viewW) * 0.5;
	double centerY = static_cast<double>(viewH) * 0.5;
	bool solved = false;

	auto samePoint = [](const CPoint& a, const CPoint& b) -> bool
		{
			return a.x == b.x && a.y == b.y;
		};

	if (samePoint(p0, p1))
	{
		centerX = static_cast<double>(p0.x);
		centerY = static_cast<double>(p0.y);
		solved = true;
	}
	else
	{
		const double x1 = static_cast<double>(p0.x);
		const double y1 = static_cast<double>(p0.y);
		const double x2 = static_cast<double>(p2.x);
		const double y2 = static_cast<double>(p2.y);
		const double x3 = static_cast<double>(p1.x);
		const double y3 = static_cast<double>(p1.y);
		const double x4 = static_cast<double>(p3.x);
		const double y4 = static_cast<double>(p3.y);

		const double den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
		if (fabs(den) > 1e-9)
		{
			const double det1 = x1 * y2 - y1 * x2;
			const double det2 = x3 * y4 - y3 * x4;
			centerX = (det1 * (x3 - x4) - (x1 - x2) * det2) / den;
			centerY = (det1 * (y3 - y4) - (y1 - y2) * det2) / den;
			solved = true;
		}
	}

	if (!solved)
	{
		centerX = (static_cast<double>(p0.x) + static_cast<double>(p1.x) + static_cast<double>(p2.x) + static_cast<double>(p3.x)) / 4.0;
		centerY = (static_cast<double>(p0.y) + static_cast<double>(p1.y) + static_cast<double>(p2.y) + static_cast<double>(p3.y)) / 4.0;
	}

	const int solvedX = static_cast<int>(std::lround(centerX));
	const int solvedY = static_cast<int>(std::lround(centerY));

	SetCrosshairCalibrationOffset(
		solvedX - (viewW / 2),
		solvedY - (viewH / 2));

	if (fabs(static_cast<double>(p0.x - p2.x)) < 1e-9)
	{
		m_crosshairCalibrationLinePoints[0] = CPoint(p0.x, 0);
		m_crosshairCalibrationLinePoints[1] = CPoint(p0.x, viewH);
	}
	else
	{
		const double a = (static_cast<double>(p0.y) - static_cast<double>(p2.y)) /
			(static_cast<double>(p0.x) - static_cast<double>(p2.x));
		const double b = static_cast<double>(p0.y) - (a * static_cast<double>(p0.x));
		m_crosshairCalibrationLinePoints[0] = CPoint(0, static_cast<int>(std::lround(b)));
		m_crosshairCalibrationLinePoints[1] = CPoint(viewW, static_cast<int>(std::lround(a * viewW + b)));
	}

	if (fabs(static_cast<double>(p1.x - p3.x)) < 1e-9)
	{
		m_crosshairCalibrationLinePoints[2] = CPoint(p1.x, 0);
		m_crosshairCalibrationLinePoints[3] = CPoint(p1.x, viewH);
	}
	else
	{
		const double c = (static_cast<double>(p1.y) - static_cast<double>(p3.y)) /
			(static_cast<double>(p1.x) - static_cast<double>(p3.x));
		const double d = static_cast<double>(p1.y) - (c * static_cast<double>(p1.x));
		m_crosshairCalibrationLinePoints[2] = CPoint(0, static_cast<int>(std::lround(d)));
		m_crosshairCalibrationLinePoints[3] = CPoint(viewW, static_cast<int>(std::lround(c * viewW + d)));
	}

	SaveCrosshairCalibration();
	m_nCrosshairCalibrationPointCount = 0;
	m_bCrosshairCalibrationMode = false;
	return true;
}

CPoint CToruss2Dlg::GetCrosshairCalibrationOffset() const
{
	return m_ptCrosshairOffset;
}

CPoint CToruss2Dlg::GetCalibratedViewCenter(int viewW, int viewH) const
{
	return CPoint((viewW / 2) + m_ptCrosshairOffset.x, (viewH / 2) + m_ptCrosshairOffset.y);
}

void CToruss2Dlg::SetCrosshairCalibrationMode(bool enable)
{
	m_bCrosshairCalibrationMode = enable;
	ResetCrosshairCalibrationPoints();
	if (enable)
		m_bCrosshairCalibrationLineDraw = false;
}

bool CToruss2Dlg::IsCrosshairCalibrationMode() const
{
	return m_bCrosshairCalibrationMode;
}

void CToruss2Dlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.

	CDialogEx::OnLButtonDown(nFlags, point);
}

HWND CToruss2Dlg::GetCurrentMiniMapHwnd() const
{
	if (m_VideoView.IsMiniMapOnColorSub())
		return m_Video0_ColorCam.GetSafeHwnd();

	if (m_VideoView.IsMiniMapOnThermalSub())
		return m_Video1_ThermalCam.GetSafeHwnd();

	if (m_VideoView.IsMiniMapOnMiniSub())
		return m_Video2_MiniMap.GetSafeHwnd();

	return nullptr;
}

bool CToruss2Dlg::IsMiniMapSubviewHwnd(HWND hWnd) const
{
	HWND hMiniMapCurrent = GetCurrentMiniMapHwnd();
	return (hMiniMapCurrent != nullptr && hWnd == hMiniMapCurrent);
}


void CToruss2Dlg::UpdateJogTracking()
{
	if (!m_bJogTracking)
		return;

	const double panEps = 0.20;
	const double tiltEps = 0.20;

	double panErr = WrapPanDeg(m_targetJogPan - m_fPan);
	double tiltErr = m_targetJogTilt - m_fTilt;

	int panDir = 0;
	int tiltDir = 0;

	if (panErr > panEps)         panDir = 1;
	else if (panErr < -panEps)   panDir = -1;

	if (tiltErr > tiltEps)       tiltDir = 1;
	else if (tiltErr < -tiltEps) tiltDir = -1;

	BYTE cmd = 0x00;

	if (panDir < 0 && tiltDir > 0)       cmd = 0x0C;
	else if (panDir > 0 && tiltDir > 0)  cmd = 0x0A;
	else if (panDir < 0 && tiltDir < 0)  cmd = 0x14;
	else if (panDir > 0 && tiltDir < 0)  cmd = 0x12;
	else if (panDir < 0)                 cmd = 0x04;
	else if (panDir > 0)                 cmd = 0x02;
	else if (tiltDir > 0)                cmd = 0x08;
	else if (tiltDir < 0)                cmd = 0x10;
	else                                 cmd = 0x00;

	if (cmd == 0x00)
	{
		m_Command.SendCCB(0x00, 0x00, 0x0C, 0x0C);
		m_bJogTracking = false;
		return;
	}

	m_Command.SendCCB(0x00, cmd, (BYTE)m_targetJogPanSpeed, (BYTE)m_targetJogTiltSpeed);
}
void CToruss2Dlg::CancelJogTracking()
{
	if (!m_bJogTracking)
		return;

	m_bJogTracking = false;
	m_Command.SendCCB(0x00, 0x00, 0x0C, 0x0C);
	OutputDebugString(L"[CancelJogTracking]\r\n");
}

int CToruss2Dlg::GetManualPTZSpeed() const
{
	if (m_nManualPTZSpeed < 0)  return 0;
	if (m_nManualPTZSpeed > 63) return 63;
	return m_nManualPTZSpeed;
}

void CToruss2Dlg::ChangeManualPTZSpeed(int delta)
{
	m_nManualPTZSpeed += delta;

	if (m_nManualPTZSpeed < 0)
		m_nManualPTZSpeed = 0;
	if (m_nManualPTZSpeed > 63)
		m_nManualPTZSpeed = 63;

	CString dbg;
	dbg.Format(L"[PTZ SPEED] speed=%d (%d%%)\r\n",
		m_nManualPTZSpeed,
		GetManualPTZSpeedPercent());
	OutputDebugString(dbg);

	m_VideoView.MarkComposeDirty();
}

int CToruss2Dlg::GetManualPTZSpeedPercent() const
{
	int speed = GetManualPTZSpeed();
	return static_cast<int>(std::round((speed / 63.0) * 100.0));
}                                                         

int CToruss2Dlg::GetFocusNormalized() const
{
	int raw = m_nFocusValue;

	if (raw < 0) raw = 0;
	if (raw > 1000) raw = 1000;

	// 기존 코드와 동일: 0 -> 100, 1000 -> 0
	int value = 100 - (raw / 10);

	if (value < 0) value = 0;
	if (value > 100) value = 100;

	return value;
}
//void CToruss2Dlg::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
//{
//
//	CDialogEx::OnKeyDown(nChar, nRepCnt, nFlags);
//}

//void CToruss2Dlg::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
//{
//
//	CDialogEx::OnKeyUp(nChar, nRepCnt, nFlags);
//}

//BOOL CToruss2Dlg::ConnectCameraCtrl(const DeviceProfile& profile)
//{
//	DisconnectCameraCtrl();
//
//	CString ip = profile.color.ip;          // 일단 같은 IP 쓰고
//	int port = 9000;                     // 카메라 제어 전용 포트로 교체
//
//	if (!m_CamCtrlClient.Create())
//		return FALSE;
//
//	m_CamCtrlClient.SetSocketOption();
//	m_CamCtrlClient.SetRecvTimeout(1000);
//	m_CamCtrlClient.SetSendTimeout(1000);
//
//	int nErr = 0;
//	CStringA ipA(ip);
//
//	if (!m_CamCtrlClient.Connect(ipA, (USHORT)port, nErr))
//	{
//		CString msg;
//		msg.Format(L"[CamCtrl] connect fail ip=%s port=%d err=%d\r\n", ip, port, nErr);
//		OutputDebugString(msg);
//		m_CamCtrlClient.ClearSocket();
//		return FALSE;
//	}
//
//	m_CamCtrlClient.SetCallback(Received_CamCtrl_Data, this);
//	OutputDebugString(L"[CamCtrl] connected\r\n");
//	return TRUE;
//}

void CToruss2Dlg::DisconnectCameraCtrl()
{
	m_CamCtrlClient.StopReceiveThread();
	m_CamCtrlClient.ClearSocket();

	CSingleLock lock(&m_csCamCtrlRx, TRUE);
	m_camCtrlRxBuf.clear();
}

void Received_CamCtrl_Data(COMMAND_DATA& pData)
{
	if (g_pMainDlg)
		g_pMainDlg->OnRecvCameraCtrl(pData);
}

void CToruss2Dlg::OnRecvCameraCtrl(COMMAND_DATA& pData)
{
	if (pData.nLength <= 0)
		return;

	ParseCameraCtrlStream(pData.data, pData.nLength);
}

void CToruss2Dlg::ParseCameraCtrlStream(const BYTE* data, int len)
{
	if (!data || len <= 0)
		return;

	CSingleLock lock(&m_csCamCtrlRx, TRUE);
	m_camCtrlRxBuf.insert(m_camCtrlRxBuf.end(), data, data + len);

	while (true)
	{
		if (m_camCtrlRxBuf.size() < 2)
			return;

		size_t pos = 0;
		bool found = false;
		for (; pos + 1 < m_camCtrlRxBuf.size(); ++pos)
		{
			if (m_camCtrlRxBuf[pos] == 0x99 && m_camCtrlRxBuf[pos + 1] == 0x55)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			BYTE last = m_camCtrlRxBuf.back();
			m_camCtrlRxBuf.clear();
			m_camCtrlRxBuf.push_back(last);
			return;
		}

		if (pos > 0)
			m_camCtrlRxBuf.erase(m_camCtrlRxBuf.begin(), m_camCtrlRxBuf.begin() + pos);

		if (m_camCtrlRxBuf.size() < 7)
			return;

		if (m_camCtrlRxBuf[6] != 0xFF)
		{
			m_camCtrlRxBuf.erase(m_camCtrlRxBuf.begin());
			continue;
		}

		BYTE pkt[7];
		memcpy(pkt, m_camCtrlRxBuf.data(), 7);

		ParseCameraCtrlPacket7(pkt);

		m_camCtrlRxBuf.erase(m_camCtrlRxBuf.begin(), m_camCtrlRxBuf.begin() + 7);
	}
}

void CToruss2Dlg::ParseCameraCtrlPacket7(const BYTE pkt[7])
{
	BYTE cmd = pkt[2];
	BYTE data1 = pkt[3];
	BYTE p1 = pkt[4];
	BYTE p2 = pkt[5];

	CString msg;
	msg.Format(L"[CamCtrl RX] cmd=0x%02X d1=0x%02X p1=0x%02X p2=0x%02X\r\n",
		cmd, data1, p1, p2);
	OutputDebugString(msg);

	switch (cmd)
	{
	case 0x47: // Zoom Position Response
	{
		int zoomPos = ((int)p1 << 8) | (int)p2;
		m_nCamCtrlZoomPos = zoomPos;
		m_bCamCtrlZoomValid = true;

		// 화면 다시 그리기
		m_VideoView.MarkComposeDirty();
		break;
	}

	case 0x48: // Focus Position Response
	{
		int focusPos = ((int)p1 << 8) | (int)p2;
		m_nCamCtrlFocusPos = focusPos;
		m_bCamCtrlFocusValid = true;

		// 화면 다시 그리기
		m_VideoView.MarkComposeDirty();
		break;
	}

	default:
		break;
	}
}

static BYTE MakeWrapChecksum(const BYTE* data, int len)
{
	BYTE sum = 0;
	for (int i = 0; i < len; ++i)
		sum = (BYTE)((sum + data[i]) & 0xFF);
	return sum;
}

BOOL CToruss2Dlg::SendCameraViscaPacket(const BYTE* visca, int viscaLen)
{
	if (!visca || viscaLen <= 0)
		return FALSE;

	if (!m_CamCtrlClient.IsConnected())
		return FALSE;

	const int totalLen = 5 + viscaLen + 1;
	std::vector<BYTE> pkt(totalLen, 0);

	pkt[0] = 0xFF;
	pkt[1] = 0x01;
	pkt[2] = 0x44;
	pkt[3] = 0x77;
	pkt[4] = (BYTE)viscaLen;

	memcpy(&pkt[5], visca, viscaLen);

	// Byte2 ~ Byte n
	pkt[totalLen - 1] = MakeWrapChecksum(&pkt[1], totalLen - 2);

	return m_CamCtrlClient.SendPacket(0, (char*)pkt.data(), totalLen);
}

BOOL CToruss2Dlg::SendZoomPositionInquiry()
{
	BYTE visca[5] = { 0x81, 0x09, 0x04, 0x47, 0xFF };
	return SendCameraViscaPacket(visca, 5);
}

BOOL CToruss2Dlg::SendFocusPositionInquiry()
{
	BYTE visca[5] = { 0x81, 0x09, 0x04, 0x48, 0xFF };
	return SendCameraViscaPacket(visca, 5);
}
CToruss2Dlg::KEY_TARGET_VIEW CToruss2Dlg::GetCurrentKeyTargetView() const
{
	const auto src = m_VideoView.GetMainSource();

	switch (src)
	{
	case CVideoView::VIEW_SOURCE::COLOR:
		return KEY_TARGET_VIEW::COLOR;

	case CVideoView::VIEW_SOURCE::THERMAL:
		return KEY_TARGET_VIEW::THERMAL;

	default:
		return KEY_TARGET_VIEW::NONE;
	}
}

bool CToruss2Dlg::IsThermalMainView() const
{
	return (m_VideoView.GetMainSource() == CVideoView::VIEW_SOURCE::THERMAL);
}

int CToruss2Dlg::GetMainViewZoomValue() const
{
	return IsThermalMainView() ? m_nThermalZoom : m_nZoom;
}

int CToruss2Dlg::GetMainViewFocusValue() const
{
	return IsThermalMainView() ? m_nThermalFocus : GetFocusNormalized();
}

CString CToruss2Dlg::GetMainViewZoomText() const
{
	CString s;

	if (IsThermalMainView())
	{
		// Thermal은 화각 계산 안 함
		s.Format(L"Z %d", m_nThermalZoom);
	}
	else
	{
		const double maxHFov = g_zoomFovTable[0].hfov;
		const double curHFov = (m_fHFOV <= 0.0001) ? 0.0001 : m_fHFOV;
		const double mag = maxHFov / curHFov;
		s.Format(L"%.2fx", mag);
	}

	return s;
}

CString CToruss2Dlg::GetMainViewFocusText() const
{
	CString s;
	s.Format(L"F %d", GetMainViewFocusValue());
	return s;
}

void CToruss2Dlg::UpdateKeyboardMoveCommand()
{
	int panDir = 0;   // -1: left, +1: right
	int tiltDir = 0;  // +1: up, -1: down

	if (m_bMoveKeyDownLeft && !m_bMoveKeyDownRight)
		panDir = -1;
	else if (!m_bMoveKeyDownLeft && m_bMoveKeyDownRight)
		panDir = +1;

	if (m_bMoveKeyDownUp && !m_bMoveKeyDownDown)
		tiltDir = +1;
	else if (!m_bMoveKeyDownUp && m_bMoveKeyDownDown)
		tiltDir = -1;

	BYTE cmd = 0x00;

	if (panDir < 0 && tiltDir > 0)       cmd = 0x0C; // up-left
	else if (panDir > 0 && tiltDir > 0)  cmd = 0x0A; // up-right
	else if (panDir < 0 && tiltDir < 0)  cmd = 0x14; // down-left
	else if (panDir > 0 && tiltDir < 0)  cmd = 0x12; // down-right
	else if (panDir < 0)                 cmd = 0x04; // left
	else if (panDir > 0)                 cmd = 0x02; // right
	else if (tiltDir > 0)                cmd = 0x08; // up
	else if (tiltDir < 0)                cmd = 0x10; // down
	else                                 cmd = 0x00; // stop

	if (cmd == 0x00)
	{
		m_Command.SendCCB(0x00, 0x00, 0x0C, 0x0C);
	}
	else
	{
		BYTE spd = static_cast<BYTE>(GetManualPTZSpeed());
		m_Command.SendCCB(0x00, cmd, spd, spd);
	}
}

void CToruss2Dlg::UpdatePanoramaLayout(BOOL bShow)
{
	if (!m_pPanorama || !::IsWindow(m_pPanorama->GetSafeHwnd()))
		return;

	CRect rcClient;
	GetClientRect(&rcClient);

	const int width = 1920;
	const int height = 400;

	const int left = 0;
	const int top = std::max(0, rcClient.Height() - height);   // 하단 고정

	UINT swpFlags = SWP_NOZORDER | SWP_NOACTIVATE;
	if (!bShow)
		swpFlags |= SWP_HIDEWINDOW;
	else
		swpFlags |= SWP_SHOWWINDOW;

	m_pPanorama->SetWindowPos(
		nullptr,
		left, top,
		width, height,
		swpFlags);

	if (bShow)
	{
		m_pPanorama->Invalidate(FALSE);
		m_pPanorama->UpdateWindow();
	}
}