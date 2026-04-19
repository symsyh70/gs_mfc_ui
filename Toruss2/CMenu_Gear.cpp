#include "pch.h"
#include "Toruss2.h"
#include "afxdialogex.h"
#include "CMenu_Gear.h"

IMPLEMENT_DYNAMIC(CMenu_Gear, CDialogEx)

BEGIN_MESSAGE_MAP(CMenu_Gear, CDialogEx)
    ON_WM_PAINT()
    ON_WM_DESTROY()
    ON_CONTROL_RANGE(BN_CLICKED,
        IDC_DYNAMIC_BEGIN,
        IDC_DYNAMIC_END,
        &CMenu_Gear::OnClickDynamicButton)
END_MESSAGE_MAP()

CMenu_Gear::CMenu_Gear(CWnd* pParent)
    : CDialogEx(IDD_DIALOG_GEAR, pParent)
{
}

CMenu_Gear::~CMenu_Gear()
{
}

void CMenu_Gear::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BOOL CMenu_Gear::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetBackgroundColor(RGB(49, 49, 49));

    BuildTree();
    RebuildLayout();

    return TRUE;
}

void CMenu_Gear::OnDestroy()
{
    ClearTree();
    CDialogEx::OnDestroy();
}

void CMenu_Gear::OnPaint()
{
    CPaintDC dc(this);
}

UINT CMenu_Gear::AllocCtrlId()
{
    if (m_nextCtrlId > IDC_DYNAMIC_END)
        return IDC_DYNAMIC_END;

    return m_nextCtrlId++;
}

void CMenu_Gear::RegisterCtrl(UINT id, GearNode* pNode, int cmdId)
{
    m_ctrlNodeMap[id] = pNode;
    m_ctrlCmdMap[id] = cmdId;
}

void CMenu_Gear::UnregisterAllCtrls()
{
    m_ctrlNodeMap.clear();
    m_ctrlCmdMap.clear();
}

void CMenu_Gear::ClearTree()
{
    for (auto* pWnd : m_allCtrls)
    {
        if (pWnd)
        {
            if (::IsWindow(pWnd->GetSafeHwnd()))
                pWnd->DestroyWindow();
            delete pWnd;
        }
    }

    m_allCtrls.clear();
    m_roots.clear();
    UnregisterAllCtrls();
    m_nextCtrlId = IDC_DYNAMIC_BEGIN;
}

void CMenu_Gear::CreateButtonNodeControls(GearNode* pNode)
{
    if (!pNode)
        return;

    UINT id = AllocCtrlId();

    pNode->pButton = new CButton;
    pNode->pButton->Create(
        pNode->text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(0, 0, 0, 0),
        this,
        id);

    m_allCtrls.push_back(pNode->pButton);
    RegisterCtrl(id, pNode, pNode->commandId);
}

void CMenu_Gear::CreateGroupNodeControls(GearNode* pNode)
{
    if (!pNode)
        return;

    pNode->pLabel = new CStatic;
    pNode->pLabel->Create(
        pNode->text,
        WS_CHILD | WS_VISIBLE,
        CRect(0, 0, 0, 0),
        this);
    m_allCtrls.push_back(pNode->pLabel);

    pNode->pValue = new CStatic;
    pNode->pValue->Create(
        L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        CRect(0, 0, 0, 0),
        this);
    m_allCtrls.push_back(pNode->pValue);

    {
        UINT id = AllocCtrlId();
        pNode->pLeftBtn = new CButton;
        pNode->pLeftBtn->Create(
            L"▲",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(0, 0, 0, 0),
            this,
            id);

        m_allCtrls.push_back(pNode->pLeftBtn);
        RegisterCtrl(id, pNode, CMD_NONE); // 나중에 MakeGroupNode에서 덮어씀
    }

    {
        UINT id = AllocCtrlId();
        pNode->pRightBtn = new CButton;
        pNode->pRightBtn->Create(
            L"▼",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            CRect(0, 0, 0, 0),
            this,
            id);

        m_allCtrls.push_back(pNode->pRightBtn);
        RegisterCtrl(id, pNode, CMD_NONE); // 나중에 MakeGroupNode에서 덮어씀
    }

    UpdateGroupValueText(pNode);
}

CMenu_Gear::GearNode* CMenu_Gear::MakeButtonNode(const CString& text, int commandId)
{
    auto node = std::make_unique<GearNode>();
    node->text = text;
    node->type = NODE_BUTTON;
    node->commandId = commandId;

    CreateButtonNodeControls(node.get());

    GearNode* raw = node.get();
    return raw;
}

CMenu_Gear::GearNode* CMenu_Gear::MakeGroupNode(
    const CString& text,
    int initValue,
    const CString& suffix,
    int leftCmd,
    int rightCmd)
{
    auto node = std::make_unique<GearNode>();
    node->text = text;
    node->type = NODE_GROUP;
    node->valueInt = initValue;
    node->valueSuffix = suffix;

    CreateGroupNodeControls(node.get());

    if (node->pLeftBtn)
        m_ctrlCmdMap[(UINT)node->pLeftBtn->GetDlgCtrlID()] = leftCmd;
    if (node->pRightBtn)
        m_ctrlCmdMap[(UINT)node->pRightBtn->GetDlgCtrlID()] = rightCmd;

    GearNode* raw = node.get();
    return raw;
}

void CMenu_Gear::BuildTree()
{
    ClearTree();

    auto autoScan = std::make_unique<GearNode>();
    autoScan->text = L"▼ 오토 스캔";
    autoScan->type = NODE_CATEGORY;
    autoScan->expanded = true;
    autoScan->commandId = CMD_NONE;
    CreateButtonNodeControls(autoScan.get());

    {
        auto child = std::make_unique<GearNode>();
        child->text = L"정지";
        child->type = NODE_BUTTON;
        child->commandId = CMD_AUTOSCAN_STOP_TOP;
        CreateButtonNodeControls(child.get());
        autoScan->children.push_back(std::move(child));
    }

    {
        auto child = std::make_unique<GearNode>();
        child->text = L"실행";
        child->type = NODE_BUTTON;
        child->commandId = CMD_AUTOSCAN_RUN;
        CreateButtonNodeControls(child.get());
        autoScan->children.push_back(std::move(child));
    }

    {
        auto child = std::make_unique<GearNode>();
        child->text = L"정지";
        child->type = NODE_BUTTON;
        child->commandId = CMD_AUTOSCAN_STOP;
        CreateButtonNodeControls(child.get());
        autoScan->children.push_back(std::move(child));
    }

    {
        auto child = std::make_unique<GearNode>();
        child->text = L"이동 속도";
        child->type = NODE_GROUP;
        child->valueInt = 1;
        child->valueSuffix = L"";
        CreateGroupNodeControls(child.get());

        if (child->pLeftBtn)
            m_ctrlCmdMap[(UINT)child->pLeftBtn->GetDlgCtrlID()] = CMD_AUTOSCAN_SPEED_UP;
        if (child->pRightBtn)
            m_ctrlCmdMap[(UINT)child->pRightBtn->GetDlgCtrlID()] = CMD_AUTOSCAN_SPEED_DOWN;

        autoScan->children.push_back(std::move(child));
    }

    {
        auto child = std::make_unique<GearNode>();
        child->text = L"지연 시간";
        child->type = NODE_GROUP;
        child->valueInt = 1;
        child->valueSuffix = L" 초";
        CreateGroupNodeControls(child.get());

        if (child->pLeftBtn)
            m_ctrlCmdMap[(UINT)child->pLeftBtn->GetDlgCtrlID()] = CMD_AUTOSCAN_DELAY_UP;
        if (child->pRightBtn)
            m_ctrlCmdMap[(UINT)child->pRightBtn->GetDlgCtrlID()] = CMD_AUTOSCAN_DELAY_DOWN;

        autoScan->children.push_back(std::move(child));
    }

    {
        auto child = std::make_unique<GearNode>();
        child->text = L"현재 위치 추가";
        child->type = NODE_BUTTON;
        child->commandId = CMD_AUTOSCAN_ADD_POS;
        CreateButtonNodeControls(child.get());
        autoScan->children.push_back(std::move(child));
    }

    {
        auto child = std::make_unique<GearNode>();
        child->text = L"선택 위치 삭제";
        child->type = NODE_BUTTON;
        child->commandId = CMD_AUTOSCAN_DEL_POS;
        CreateButtonNodeControls(child.get());
        autoScan->children.push_back(std::move(child));
    }

    {
        auto child = std::make_unique<GearNode>();
        child->text = L"▼";
        child->type = NODE_BUTTON;
        child->commandId = CMD_AUTOSCAN_DROPDOWN;
        CreateButtonNodeControls(child.get());
        autoScan->children.push_back(std::move(child));
    }

    m_roots.push_back(std::move(autoScan));
}

void CMenu_Gear::UpdateGroupValueText(GearNode* pNode)
{
    if (!pNode || pNode->type != NODE_GROUP || !pNode->pValue)
        return;

    CString s;
    s.Format(L"%d%s", pNode->valueInt, pNode->valueSuffix.GetString());
    pNode->pValue->SetWindowTextW(s);
}

void CMenu_Gear::HideNodeRecursive(GearNode* pNode)
{
    if (!pNode)
        return;

    if (pNode->pButton)
        pNode->pButton->ShowWindow(SW_HIDE);

    if (pNode->pLabel)
        pNode->pLabel->ShowWindow(SW_HIDE);

    if (pNode->pValue)
        pNode->pValue->ShowWindow(SW_HIDE);

    if (pNode->pLeftBtn)
        pNode->pLeftBtn->ShowWindow(SW_HIDE);

    if (pNode->pRightBtn)
        pNode->pRightBtn->ShowWindow(SW_HIDE);

    for (auto& child : pNode->children)
        HideNodeRecursive(child.get());
}

int CMenu_Gear::LayoutNode(GearNode* pNode, int x, int y, int depth)
{
    if (!pNode)
        return y;

    const int indent = depth * INDENT_W;
    const int left = x + indent;
    const int width = BTN_W - indent;

    if (pNode->type == NODE_CATEGORY || pNode->type == NODE_BUTTON)
    {
        if (pNode->pButton)
        {
            pNode->pButton->MoveWindow(left, y, width, BTN_H);
            pNode->pButton->ShowWindow(SW_SHOW);
        }

        y += BTN_H + GAP_Y;
    }
    else if (pNode->type == NODE_GROUP)
    {
        const int labelY = y;
        const int rowY = y + GROUP_LABEL_H + 4;

        if (pNode->pLabel)
        {
            pNode->pLabel->MoveWindow(left, labelY, width, GROUP_LABEL_H);
            pNode->pLabel->ShowWindow(SW_SHOW);
        }

        if (pNode->pLeftBtn)
        {
            pNode->pLeftBtn->MoveWindow(left, rowY, GROUP_SIDE_W, GROUP_ROW_H);
            pNode->pLeftBtn->ShowWindow(SW_SHOW);
        }

        if (pNode->pValue)
        {
            pNode->pValue->MoveWindow(
                left + GROUP_SIDE_W + GROUP_INNER_GAP,
                rowY,
                GROUP_VALUE_W,
                GROUP_ROW_H);
            pNode->pValue->ShowWindow(SW_SHOW);
        }

        if (pNode->pRightBtn)
        {
            pNode->pRightBtn->MoveWindow(
                left + GROUP_SIDE_W + GROUP_INNER_GAP + GROUP_VALUE_W + GROUP_INNER_GAP,
                rowY,
                GROUP_SIDE_W,
                GROUP_ROW_H);
            pNode->pRightBtn->ShowWindow(SW_SHOW);
        }

        y += GROUP_H + GAP_Y;
    }

    if (pNode->type == NODE_CATEGORY && !pNode->expanded)
    {
        for (auto& child : pNode->children)
            HideNodeRecursive(child.get());

        return y;
    }

    for (auto& child : pNode->children)
        y = LayoutNode(child.get(), x, y, depth + 1);

    return y;
}

void CMenu_Gear::RebuildLayout()
{
    int curY = START_Y;

    for (auto& root : m_roots)
        curY = LayoutNode(root.get(), START_X, curY, 0);

    Invalidate(FALSE);
    UpdateWindow();
}

CMenu_Gear::GearNode* CMenu_Gear::FindNodeByCtrlId(UINT nID) const
{
    auto it = m_ctrlNodeMap.find(nID);
    if (it == m_ctrlNodeMap.end())
        return nullptr;

    return it->second;
}

int CMenu_Gear::FindCommandByCtrlId(UINT nID) const
{
    auto it = m_ctrlCmdMap.find(nID);
    if (it == m_ctrlCmdMap.end())
        return CMD_NONE;

    return it->second;
}

void CMenu_Gear::HandleCommand(int commandId, GearNode* pNode)
{
    switch (commandId)
    {
    case CMD_AUTOSCAN_STOP_TOP:
        AfxMessageBox(L"상단 정지");
        break;

    case CMD_AUTOSCAN_RUN:
        AfxMessageBox(L"실행");
        break;

    case CMD_AUTOSCAN_STOP:
        AfxMessageBox(L"정지");
        break;

    case CMD_AUTOSCAN_SPEED_UP:
        if (pNode)
        {
            if (pNode->valueInt < 63)
                pNode->valueInt++;
            UpdateGroupValueText(pNode);
        }
        break;

    case CMD_AUTOSCAN_SPEED_DOWN:
        if (pNode)
        {
            if (pNode->valueInt > 1)
                pNode->valueInt--;
            UpdateGroupValueText(pNode);
        }
        break;

    case CMD_AUTOSCAN_DELAY_UP:
        if (pNode)
        {
            pNode->valueInt++;
            UpdateGroupValueText(pNode);
        }
        break;

    case CMD_AUTOSCAN_DELAY_DOWN:
        if (pNode)
        {
            if (pNode->valueInt > 1)
                pNode->valueInt--;
            UpdateGroupValueText(pNode);
        }
        break;

    case CMD_AUTOSCAN_ADD_POS:
        AfxMessageBox(L"현재 위치 추가");
        break;

    case CMD_AUTOSCAN_DEL_POS:
        AfxMessageBox(L"선택 위치 삭제");
        break;

    case CMD_AUTOSCAN_DROPDOWN:
        AfxMessageBox(L"하단 ▼");
        break;

    default:
        break;
    }
}

void CMenu_Gear::OnClickDynamicButton(UINT nID)
{
    GearNode* pNode = FindNodeByCtrlId(nID);
    if (!pNode)
        return;

    const int cmd = FindCommandByCtrlId(nID);

    if (pNode->type == NODE_CATEGORY && pNode->pButton &&
        (UINT)pNode->pButton->GetDlgCtrlID() == nID)
    {
        pNode->expanded = !pNode->expanded;

        CString text = pNode->text;
        if (text.GetLength() >= 2)
            text.SetAt(1, pNode->expanded ? L'▼' : L'▶');

        pNode->text = text;
        pNode->pButton->SetWindowTextW(text);

        RebuildLayout();
        return;
    }

    HandleCommand(cmd, pNode);
}