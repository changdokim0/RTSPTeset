#pragma once
#include "afxdialogex.h"


// FiloaderDialog 대화 상자

class FiloaderDialog : public CDialogEx
{
	DECLARE_DYNAMIC(FiloaderDialog)

public:
	FiloaderDialog(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~FiloaderDialog();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_FILELOADER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	DECLARE_MESSAGE_MAP()
};
