// CPanel_AutoScan.cpp: 구현 파일
//

#include "pch.h"
#include "Toruss2.h"
#include "afxdialogex.h"
#include "CPanel_AutoScan.h"


// CPanel_AutoScan 대화 상자

IMPLEMENT_DYNAMIC(CPanel_AutoScan, CDialogEx)

CPanel_AutoScan::CPanel_AutoScan(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_PANEL_AUTOSCAN, pParent)
{

}

CPanel_AutoScan::~CPanel_AutoScan()
{
}

void CPanel_AutoScan::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CPanel_AutoScan, CDialogEx)
END_MESSAGE_MAP()


// CPanel_AutoScan 메시지 처리기
