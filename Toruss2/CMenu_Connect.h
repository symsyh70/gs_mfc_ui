#pragma once
#include <vector>
#include "DeviceProfile.h"
#include "CConnectButton.h"

class CToruss2Dlg;

class CMenu_Connect : public CDialogEx
{
public:
    CMenu_Connect(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIALOG_CONNECT };
#endif

    void SetMainDlg(CToruss2Dlg* pMain);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    afx_msg void OnDestroy();
    afx_msg void OnClickDeviceButton(UINT nID);

    DECLARE_MESSAGE_MAP()

private:
    void BuildDeviceButtons();
    void ClearDeviceButtons();

private:
    CToruss2Dlg* m_pMainDlg = nullptr;
    std::vector<DeviceListItem> m_items;
    std::vector<CConnectButton*> m_buttons;

    enum { IDC_DEVICE_BTN_BASE = 60000 };
};