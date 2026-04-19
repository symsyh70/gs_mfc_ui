#pragma once
#include "afxdialogex.h"


// CControl_PowerManager 대화 상자

class CControl_PowerManager : public CDialogEx
{
	DECLARE_DYNAMIC(CControl_PowerManager)

public:
	CControl_PowerManager(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~CControl_PowerManager();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PANEL_POWERMANAGER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	DECLARE_MESSAGE_MAP()

public:
	//void Send
};
