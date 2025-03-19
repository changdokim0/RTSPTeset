// FiloaderDialog.cpp: 구현 파일
//

#include "pch.h"
#include "RTSPTester1.h"
#include "afxdialogex.h"
#include "FiloaderDialog.h"


// FiloaderDialog 대화 상자

IMPLEMENT_DYNAMIC(FiloaderDialog, CDialogEx)

FiloaderDialog::FiloaderDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_FILELOADER, pParent)
{

}

FiloaderDialog::~FiloaderDialog()
{
}

void FiloaderDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(FiloaderDialog, CDialogEx)
END_MESSAGE_MAP()


// FiloaderDialog 메시지 처리기
