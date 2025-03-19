#pragma once
#include "afxdialogex.h"
#include <vector>
#include <string>
#include <thread>
#include "RTSPConnector.h"


// CRtspReceiver 대화 상자

class CRtspReceiver : public CDialogEx
{
	DECLARE_DYNAMIC(CRtspReceiver)

public:
	CRtspReceiver(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~CRtspReceiver();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_RTSP_RECV_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedBtnConnect();
	afx_msg void OnBnClickedBtnDisconnect();
	afx_msg void OnBnClickedBtnCsvConnect();
	afx_msg void OnRangeRadioGroup1(UINT uID);

	//CTRL
	CListCtrl m_Frame_info;
	CEdit m_SessionCount;
	CIPAddressCtrl m_IPControl;
	CEdit m_DeviceUUID;
	CEdit m_csv_filename;
	CEdit m_csv_addr;
	CEdit m_begin_index;
	CEdit m_Edt_ServerPort;
	CEdit m_Edt_Connect_Timeout;

	LRESULT test(WPARAM wparam, LPARAM lparam);


	//Variable
	bool run_thread_ = false;
	CString strPathIni;
	std::vector<std::shared_ptr<RTSPConnector>> rtsp_testers;
	std::vector<std::thread*> rtsp_treads_;
	std::vector<std::string>  GetRTSPaddrFromCSV(std::string str_csv_filename, std::string str_csv_addr);
	std::vector<std::vector<std::shared_ptr<RTSPConnector>>> SplitVector(const std::vector<std::shared_ptr<RTSPConnector>>& inputVector, size_t threadSize);
public:
	virtual BOOL OnInitDialog();

};
