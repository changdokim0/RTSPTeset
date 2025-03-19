
// RTSPTester1Dlg.h: 헤더 파일
//

#pragma once
#include "RTSPConnector.h"
#include <vector>
#include <memory>
#include <thread>
#include "CRtspReceiver.h"
#include "CFileLoaderTab.h"


// CRTSPTester1Dlg 대화 상자
class CRTSPTester1Dlg : public CDialogEx
{
// 생성입니다.
public:
	CRTSPTester1Dlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_RTSPTESTER1_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult);
	//void SendTest(HWND hwnd);
	//void PostTest(HWND hwnd);


	CEdit m_Edt_ServerPort;


	CTabCtrl m_tabCtrl;
	CRtspReceiver* RtspReceiver;
	CFileLoaderTab* FileLoaderTab;
};

