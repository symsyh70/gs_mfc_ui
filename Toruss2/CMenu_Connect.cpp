#include "pch.h"
#include "resource.h"
#include "CMenu_Connect.h"
#include "Toruss2Dlg.h"
#include "afxdialogex.h"

BEGIN_MESSAGE_MAP(CMenu_Connect, CDialogEx)
    ON_WM_DESTROY()
    ON_CONTROL_RANGE(BN_CLICKED, IDC_DEVICE_BTN_BASE, IDC_DEVICE_BTN_BASE + 999, &CMenu_Connect::OnClickDeviceButton)
END_MESSAGE_MAP()

CMenu_Connect::CMenu_Connect(CWnd* pParent)
    : CDialogEx(IDD_DIALOG_CONNECT, pParent)
{
}

void CMenu_Connect::SetMainDlg(CToruss2Dlg* pMain)
{
    m_pMainDlg = pMain;
}

void CMenu_Connect::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BOOL CMenu_Connect::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetBackgroundColor(RGB(49, 49, 49));

    BuildDeviceButtons();
    return TRUE;
}

void CMenu_Connect::ClearDeviceButtons()
{
    for (auto* pBtn : m_buttons)
    {
        if (pBtn)
        {
            pBtn->DestroyWindow();
            delete pBtn;
        }
    }
    m_buttons.clear();
    m_items.clear();
}

void CMenu_Connect::BuildDeviceButtons()
{
    ClearDeviceButtons();

    if (!m_pMainDlg)
        return;

    if (!m_pMainDlg->m_mongo.GetDeviceList(m_items))
    {
        AfxMessageBox(L"장비 목록 조회 실패");
        return;
    }

    const int startX = 40;
    const int startY = 20;
    const int btnW = 250;
    const int btnH = 34;
    const int gapY = 10;

    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        CString text;
        if (m_items[i].deviceModel.IsEmpty())
            text = m_items[i].siteName;
        else
            text.Format(L"%s (%s)", m_items[i].siteName.GetString(), m_items[i].deviceModel.GetString());

        CRect rc(startX, startY + i * (btnH + gapY),
            startX + btnW, startY + i * (btnH + gapY) + btnH);

        CConnectButton* pBtn = new CConnectButton;
        pBtn->Create(text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rc, this, IDC_DEVICE_BTN_BASE + i);

        m_buttons.push_back(pBtn);
    }
}

void CMenu_Connect::OnClickDeviceButton(UINT nID)
{
    if (!m_pMainDlg)
        return;

    int idx = (int)nID - IDC_DEVICE_BTN_BASE;
    if (idx < 0 || idx >= (int)m_items.size())
        return;

    if (m_pMainDlg->m_isOpening)
    {
        OutputDebugString(L"[CMenu_Connect] skip click - already opening\r\n");
        return;
    }

    for (int i = 0; i < (int)m_buttons.size(); i++)
        m_buttons[i]->SetSelected(i == idx);

    DeviceProfile profile;
    if (!m_pMainDlg->m_mongo.LoadDeviceProfile(m_items[idx].unitId, profile))
    {
        AfxMessageBox(L"장비 상세 조회 실패");
        return;
    }

    std::thread([dlg = m_pMainDlg, profile]()
        {
            dlg->ConnectCCB(profile);
            dlg->OpenSelectedDevice(profile);/*
            dlg->ConnectCameraCtrl(profile);*/
        }).detach();
}

void CMenu_Connect::OnDestroy()
{
    ClearDeviceButtons();
    CDialogEx::OnDestroy();
}