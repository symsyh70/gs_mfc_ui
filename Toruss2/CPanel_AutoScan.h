#pragma once
#include "afxdialogex.h"


// CPanel_AutoScan 대화 상자

class CPanel_AutoScan : public CDialogEx
{
	DECLARE_DYNAMIC(CPanel_AutoScan)

public:
	CPanel_AutoScan(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~CPanel_AutoScan();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PANEL_AUTOSCAN };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	DECLARE_MESSAGE_MAP()
};
