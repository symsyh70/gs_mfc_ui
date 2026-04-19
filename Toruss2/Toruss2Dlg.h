
// Toruss2Dlg.h: 헤더 파일
//
#pragma once
#include "MongoDB.h"
#include "CSideButton.h"
#include "Logger.h"
#include "CMenu_Connect.h"
#include "CMenu_Gear.h"
#include "CMenu_Video.h"
#include "VideoView.h"
#include "CTopButton.h"
#include "TCP_Client.h"
#include "CControl_Panorama.h"
#include "MOTWrapper.h"
#include "RtspBuild.h"
#include "CMainView.h"
#include "CControl_Command.h"
#include "CControl_ColorCam.h"
#include "CControl_ThermalCam.h"

#include <vector>
#include <atomic>

#include <Gdiplus.h>

#define WM_VIDEO_RENDER (WM_APP + 400)

// CToruss2Dlg 대화 상자
class CToruss2Dlg : public CDialogEx
{
	// 생성입니다.
public:
	CToruss2Dlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.

	CControl_Command m_Command;

	// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TORUSS2_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.

	enum class PAGE_TYPE
	{
		CONNECT,
		GEAR,
		VIDEO
	};

	struct ZoomFovEntry
	{
		int zoom;       // 0 ~ 1000
		double hfov;    // 해당 zoom에서의 수평 화각
	};

	static const ZoomFovEntry g_zoomFovTable[];  // 선언만!!

	// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:

	enum class KEY_TARGET_VIEW
	{
		NONE,
		COLOR,
		THERMAL
	};

	KEY_TARGET_VIEW GetCurrentKeyTargetView() const;

	CControl_ColorCam m_ColorCam;
	CControl_ThermalCam m_ThermalCam;
	CVideoView m_VideoView;

	HWND GetCurrentMiniMapHwnd() const;
	bool IsMiniMapSubviewHwnd(HWND hWnd) const;

	double m_fHFOV = 63.0;
	int    m_nZoomValue = 0;
	int m_nFocusValue = 0;

	// [추가] tilt 기반 VFOV 설정값
	double m_vFovAtTiltMin = 6.0;     // tilt 최소일 때 VFOV
	double m_vFovAtTiltMax = 18.0;    // tilt 최대일 때 VFOV
	double m_tiltMinDegForVFov = -40.0;
	double m_tiltMaxDegForVFov = 40.0;

	double GetHFovFromZoom(int zoomValue);
	double GetVFovFromZoom(int zoomValue, int frameW, int frameH);
	double GetVFovFromTilt(double tiltDeg) const;
	void UpdateFovByZoom(int zoomValue);

	int GetZoomValue() const { return m_nZoomValue; }
	int GetFocusValue() const { return m_nFocusValue; }
	double GetMagnifyingPower() const;
	int GetFocusNormalized() const;

	TCP_Client m_CamCtrlClient;

	CCriticalSection m_csCamCtrlRx;
	std::vector<BYTE> m_camCtrlRxBuf;

	BOOL ConnectCameraCtrl(const DeviceProfile& profile);
	void DisconnectCameraCtrl();

	void OnRecvCameraCtrl(COMMAND_DATA& pData);
	void ParseCameraCtrlStream(const BYTE* data, int len);
	void ParseCameraCtrlPacket7(const BYTE pkt[7]);

	BOOL SendCameraViscaPacket(const BYTE* visca, int viscaLen);

public:
	int  m_nCamCtrlZoomPos = -1;
	int  m_nCamCtrlFocusPos = -1;
	bool m_bCamCtrlZoomValid = false;
	bool m_bCamCtrlFocusValid = false;

	BOOL SendZoomPositionInquiry();
	BOOL SendFocusPositionInquiry();

public:
	bool MoveCameraToPanTilt(double panDeg, double tiltDeg, int panSpeed = 20, int tiltSpeed = 20);
	void UpdateJogTracking();
	void CancelJogTracking();

	bool   m_bJogTracking = false;
	double m_targetJogPan = 0.0;
	double m_targetJogTilt = 0.0;
	int    m_targetJogPanSpeed = 20;
	int    m_targetJogTiltSpeed = 20;

	CString GetCalibrationIniPath() const;
	void LoadCrosshairCalibration();
	void SaveCrosshairCalibration() const;
	void SetCrosshairCalibrationOffset(int offsetX, int offsetY);
	bool AddCrosshairCalibrationPoint(const CPoint& clickPt, int viewW, int viewH);
	void ResetCrosshairCalibrationPoints();
	int GetCrosshairCalibrationPointCount() const;
	CPoint GetCrosshairCalibrationPoint(int index) const;
	bool HasCrosshairCalibrationLines() const;
	void GetCrosshairCalibrationLine(int lineIndex, CPoint& outStart, CPoint& outEnd) const;
	CPoint GetCrosshairCalibrationOffset() const;
	CPoint GetCalibratedViewCenter(int viewW, int viewH) const;
	void SetCrosshairCalibrationMode(bool enable);
	bool IsCrosshairCalibrationMode() const;

	std::atomic<bool> m_bRendering = false;
	MOTWrapper m_DeepAI;
	CMongoManager m_mongo;
	BOOL m_bInit;
	Gdiplus::Image* m_pLogo;
	CBrush m_brStatBg;
	static CToruss2Dlg* g_pFrameView;
	CSideButton m_btn_ConnectList;
	CSideButton m_btn_GearControl;
	CSideButton m_btn_VideoControl;
	CTopButton m_btn_Panorama;
	CControl_Panorama* m_pPanorama = nullptr;

	void UpdatePanoramaLayout(BOOL bShow);

	afx_msg LRESULT OnVideoRender(WPARAM wParam, LPARAM lParam);

	CMenu_Connect m_MenuConnect;

	std::vector<CSideButton*> m_menuButtons;

private:
	void OnMenuButton(UINT nID);

	CMenu_Connect m_pageConnect;
	CMenu_Gear m_pageGear;
	CMenu_Video m_pageVideo;

	PAGE_TYPE m_currentPage;
	BOOL CreatePanoramaPage();
	void TogglePanoramaPage();
	bool m_panorama_onoff;

	public:
		int m_nManualPTZSpeed = 20;   // 0 ~ 63
		bool m_bShowManualPTZSpeed = false;
		bool m_bSpeedKeyDownZ = false;
		bool m_bSpeedKeyDownC = false;

		int GetManualPTZSpeed() const;
		void ChangeManualPTZSpeed(int delta);
		int GetManualPTZSpeedPercent() const;
		bool IsManualPTZSpeedVisible() const { return m_bShowManualPTZSpeed; }

			void SendZoomStep(int delta);
			void SendFocusStep(int delta);

			bool m_bZoomKeyDownW = false;
			bool m_bZoomKeyDownS = false;
			bool m_bFocusKeyDownA = false;
			bool m_bFocusKeyDownD = false;

			bool m_bMoveKeyDownUp = false;
			bool m_bMoveKeyDownLeft = false;
			bool m_bMoveKeyDownRight = false;
			bool m_bMoveKeyDownDown = false;

			TCP_Client m_CamCtrlSocket;
			void UpdateKeyboardMoveCommand();

public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	CStatic m_Logo;
	CStatic m_Video0_ColorCam;
	CStatic m_Video1_ThermalCam;
	CStatic m_Video2_MiniMap;
	CStatic m_PageArea;

		double m_fUnitLat = 36.350400;   // 기본값 예시
		double m_fUnitLon = 127.384500;

		bool m_bMiniMapMeasureDragging = false;

	std::atomic<bool> m_isOpening = false;

	BOOL ConnectCCB(const DeviceProfile& profile);

	void OpenSelectedDevice(const DeviceProfile& profile);

	void ParseAndSendCommand(COMMAND_DATA& pData);

	void InitPages();
	void ShowPage(PAGE_TYPE type);
	CMainVideoView m_MainView;
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnHotKey(UINT nHotKeyId, UINT nKey1, UINT nKey2);

public:
	void ToggleMainFullscreen();
	bool m_bMiniMapDragging = false;
	CPoint m_ptMiniMapLast = CPoint(0, 0);
public:
	bool m_bMainFullscreen = false;

	CRect m_rcColor;
	CRect m_rcThermal;
	CRect m_rcMiniMap;
	CRect m_rcMain;

	double m_fPan = 0.0;
	double m_fTilt = 0.0;
	int m_nZoom = 0;
	int m_nFocus = 0;
	int m_nPower = 0;

	int m_nThermalZoom = 0;
	int m_nThermalFocus = 0;

	bool m_bCrosshairCalibrationMode = false;
	bool m_bCrosshairCalibrationLineDraw = false;
	int m_nCrosshairCalibrationPointCount = 0;
	CPoint m_ptCrosshairOffset = CPoint(0, 0);
	CPoint m_crosshairCalibrationPoints[4] =
	{
		CPoint(-3, -3),
		CPoint(-3, -3),
		CPoint(-3, -3),
		CPoint(-3, -3)
	};
	CPoint m_crosshairCalibrationLinePoints[4] =
	{
		CPoint(-3, -3),
		CPoint(-3, -3),
		CPoint(-3, -3),
		CPoint(-3, -3)
	};

	void OnCCBTelemetry(double panDeg, double tiltDeg, int zoomPos, int focusPos, int powerStatus);
	CStatic m_PanDeg;
	CStatic m_TiltDeg;
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
public:
	bool IsThermalMainView() const;
	int GetMainViewZoomValue() const;
	int GetMainViewFocusValue() const;
	CString GetMainViewZoomText() const;
	CString GetMainViewFocusText() const;
};



