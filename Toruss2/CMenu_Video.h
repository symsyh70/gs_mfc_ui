#pragma once
#include "afxdialogex.h"

class CControl_ColorCam;
class CControl_ThermalCam;

class CMenu_Video : public CDialogEx
{
    DECLARE_DYNAMIC(CMenu_Video)

public:
    CMenu_Video(CWnd* pParent = nullptr);
    virtual ~CMenu_Video();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIALOG_VIDEO };
#endif

public:
    void SetSubDialogs(CControl_ColorCam* pDayCam, CControl_ThermalCam* pThermalCam);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    afx_msg void OnDestroy();
    afx_msg void OnPaint();
    afx_msg void OnClickCategoryButton(UINT nID);

    DECLARE_MESSAGE_MAP()

private:
    enum
    {
        IDC_BTN_DAYCAM = 61000,
        IDC_BTN_THERMALCAM,

        BTN_X = 12,
        BTN_Y = 10,
        BTN_W = 200,
        BTN_H = 30,
        BTN_GAP = 6
    };

    enum VIDEO_CATEGORY
    {
        VIDEO_DAYCAM = 0,
        VIDEO_THERMALCAM
    };

private:
    CButton m_btnDayCam;
    CButton m_btnThermalCam;

    CControl_ColorCam* m_pDlgDayCam = nullptr;
    CControl_ThermalCam* m_pDlgThermalCam = nullptr;

    VIDEO_CATEGORY m_currentCategory = VIDEO_DAYCAM;

private:
    void CreateCategoryButtons();
    void LayoutCategoryButtons();
    void ShowCategory(VIDEO_CATEGORY type);
    void UpdateCategoryButtons();
};