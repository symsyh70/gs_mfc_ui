// CControl_PowerManager.cpp: 구현 파일
//

#include "pch.h"
#include "Toruss2.h"
#include "afxdialogex.h"
#include "CControl_PowerManager.h"


// CControl_PowerManager 대화 상자

IMPLEMENT_DYNAMIC(CControl_PowerManager, CDialogEx)

CControl_PowerManager::CControl_PowerManager(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_PANEL_POWERMANAGER, pParent)
{

}

CControl_PowerManager::~CControl_PowerManager()
{
}

void CControl_PowerManager::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CControl_PowerManager, CDialogEx)
END_MESSAGE_MAP()


// CControl_PowerManager 메시지 처리기
