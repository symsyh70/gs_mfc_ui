#include "pch.h"
#include "Toruss2.h"
#include "afxdialogex.h"
#include "CMenu_Video.h"
#include "CControl_ColorCam.h"
#include "CControl_ThermalCam.h"

IMPLEMENT_DYNAMIC(CMenu_Video, CDialogEx)

BEGIN_MESSAGE_MAP(CMenu_Video, CDialogEx)
    ON_WM_PAINT()
    ON_WM_DESTROY()
    ON_CONTROL_RANGE(BN_CLICKED, IDC_BTN_DAYCAM, IDC_BTN_THERMALCAM, &CMenu_Video::OnClickCategoryButton)
END_MESSAGE_MAP()

CMenu_Video::CMenu_Video(CWnd* pParent)
    : CDialogEx(IDD_DIALOG_VIDEO, pParent)
{
}

CMenu_Video::~CMenu_Video()
{
}

void CMenu_Video::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BOOL CMenu_Video::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetBackgroundColor(RGB(49, 49, 49));

    CreateCategoryButtons();
    LayoutCategoryButtons();
    UpdateCategoryButtons();

    return TRUE;
}

void CMenu_Video::OnDestroy()
{
    CDialogEx::OnDestroy();
}

void CMenu_Video::OnPaint()
{
    CPaintDC dc(this);
}

void CMenu_Video::SetSubDialogs(CControl_ColorCam* pDayCam, CControl_ThermalCam* pThermalCam)
{
    m_pDlgDayCam = pDayCam;
    m_pDlgThermalCam = pThermalCam;

    if (::IsWindow(GetSafeHwnd()))
    {
        ShowCategory(m_currentCategory);
    }
}

void CMenu_Video::CreateCategoryButtons()
{
    m_btnDayCam.Create(
        L"▶ 주간카메라",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(0, 0, 0, 0),
        this,
        IDC_BTN_DAYCAM);

    m_btnThermalCam.Create(
        L"▶ 열상카메라",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(0, 0, 0, 0),
        this,
        IDC_BTN_THERMALCAM);
}

void CMenu_Video::LayoutCategoryButtons()
{
    int y = BTN_Y;

    m_btnDayCam.MoveWindow(BTN_X, y, BTN_W, BTN_H);
    y += BTN_H + BTN_GAP;

    m_btnThermalCam.MoveWindow(BTN_X, y, BTN_W, BTN_H);
}

void CMenu_Video::UpdateCategoryButtons()
{
    m_btnDayCam.SetWindowTextW(
        m_currentCategory == VIDEO_DAYCAM ? L"▼ 주간카메라" : L"▶ 주간카메라");

    m_btnThermalCam.SetWindowTextW(
        m_currentCategory == VIDEO_THERMALCAM ? L"▼ 열상카메라" : L"▶ 열상카메라");
}

void CMenu_Video::ShowCategory(VIDEO_CATEGORY type)
{
    m_currentCategory = type;

    // 숨기기
    if (m_pDlgDayCam && ::IsWindow(m_pDlgDayCam->GetSafeHwnd()))
        m_pDlgDayCam->ShowWindow(SW_HIDE);

    if (m_pDlgThermalCam && ::IsWindow(m_pDlgThermalCam->GetSafeHwnd()))
        m_pDlgThermalCam->ShowWindow(SW_HIDE);

    // 표시할 영역 계산
    CRect rc;
    GetClientRect(&rc);

    int top = BTN_Y + (BTN_H + BTN_GAP) * 2 + 10;

    CRect rcContent(
        BTN_X,
        top,
        rc.right - 10,
        rc.bottom - 10
    );

    // 선택된 dialog 표시
    switch (type)
    {
    case VIDEO_DAYCAM:
        if (m_pDlgDayCam && ::IsWindow(m_pDlgDayCam->GetSafeHwnd()))
        {
            m_pDlgDayCam->SetParent(this);              // ⭐ 핵심 1
            m_pDlgDayCam->MoveWindow(rcContent);        // ⭐ 핵심 2
            m_pDlgDayCam->ShowWindow(SW_SHOW);
        }
        break;

    case VIDEO_THERMALCAM:
        if (m_pDlgThermalCam && ::IsWindow(m_pDlgThermalCam->GetSafeHwnd()))
        {
            m_pDlgThermalCam->SetParent(this);          // ⭐ 핵심 1
            m_pDlgThermalCam->MoveWindow(rcContent);    // ⭐ 핵심 2
            m_pDlgThermalCam->ShowWindow(SW_SHOW);
        }
        break;
    }

    UpdateCategoryButtons();
}

void CMenu_Video::OnClickCategoryButton(UINT nID)
{
    switch (nID)
    {
    case IDC_BTN_DAYCAM:
        ShowCategory(VIDEO_DAYCAM);
        break;

    case IDC_BTN_THERMALCAM:
        ShowCategory(VIDEO_THERMALCAM);
        break;
    }
}