#pragma once
#include "afxdialogex.h"
#include <vector>
#include <memory>
#include <map>

class CMenu_Gear : public CDialogEx
{
    DECLARE_DYNAMIC(CMenu_Gear)

public:
    CMenu_Gear(CWnd* pParent = nullptr);
    virtual ~CMenu_Gear();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIALOG_GEAR };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    afx_msg void OnDestroy();
    afx_msg void OnPaint();
    afx_msg void OnClickDynamicButton(UINT nID);

    DECLARE_MESSAGE_MAP()

private:
    enum NODE_TYPE
    {
        NODE_CATEGORY = 0,
        NODE_BUTTON,
        NODE_GROUP
    };

    enum CMD_ID
    {
        CMD_NONE = 0,

        CMD_AUTOSCAN_STOP_TOP,
        CMD_AUTOSCAN_RUN,
        CMD_AUTOSCAN_STOP,

        CMD_AUTOSCAN_SPEED_UP,
        CMD_AUTOSCAN_SPEED_DOWN,

        CMD_AUTOSCAN_DELAY_UP,
        CMD_AUTOSCAN_DELAY_DOWN,

        CMD_AUTOSCAN_ADD_POS,
        CMD_AUTOSCAN_DEL_POS,
        CMD_AUTOSCAN_DROPDOWN
    };

    struct GearNode
    {
        CString text;
        NODE_TYPE type = NODE_BUTTON;
        int commandId = CMD_NONE;
        bool expanded = false;

        // CATEGORY / BUTTON
        CButton* pButton = nullptr;

        // GROUP
        CStatic* pLabel = nullptr;
        CStatic* pValue = nullptr;
        CButton* pLeftBtn = nullptr;
        CButton* pRightBtn = nullptr;

        int valueInt = 0;
        CString valueSuffix;

        std::vector<std::unique_ptr<GearNode>> children;
    };

private:
    enum
    {
        IDC_DYNAMIC_BEGIN = 61000,
        IDC_DYNAMIC_END = 65000,

        START_X = 10,
        START_Y = 10,

        BTN_W = 250,
        BTN_H = 28,

        GROUP_LABEL_H = 18,
        GROUP_ROW_H = 28,
        GROUP_H = 18 + 4 + 28,

        GAP_Y = 6,
        INDENT_W = 16,

        GROUP_SIDE_W = 70,
        GROUP_VALUE_W = 60,
        GROUP_INNER_GAP = 6
    };

private:
    std::vector<std::unique_ptr<GearNode>> m_roots;
    std::vector<CWnd*> m_allCtrls;

    std::map<UINT, GearNode*> m_ctrlNodeMap;
    std::map<UINT, int> m_ctrlCmdMap;

    UINT m_nextCtrlId = IDC_DYNAMIC_BEGIN;

private:
    void BuildTree();
    void ClearTree();

    void CreateButtonNodeControls(GearNode* pNode);
    void CreateGroupNodeControls(GearNode* pNode);

    UINT AllocCtrlId();

    void RegisterCtrl(UINT id, GearNode* pNode, int cmdId);
    void UnregisterAllCtrls();

    int LayoutNode(GearNode* pNode, int x, int y, int depth);
    void RebuildLayout();
    void HideNodeRecursive(GearNode* pNode);

    GearNode* FindNodeByCtrlId(UINT nID) const;
    int FindCommandByCtrlId(UINT nID) const;

    void HandleCommand(int commandId, GearNode* pNode);
    void UpdateGroupValueText(GearNode* pNode);

    GearNode* MakeButtonNode(const CString& text, int commandId);
    GearNode* MakeGroupNode(const CString& text, int initValue, const CString& suffix,
        int leftCmd, int rightCmd);
};