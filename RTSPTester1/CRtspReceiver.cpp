// CRtspReceiver.cpp: 구현 파일
//

#include "pch.h"
#include "RTSPTester1.h"
#include "afxdialogex.h"
#include "CRtspReceiver.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <thread>
#include <string>
#include <mutex>
#include <sstream>


// CRtspReceiver 대화 상자

IMPLEMENT_DYNAMIC(CRtspReceiver, CDialogEx)

CRtspReceiver::CRtspReceiver(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_RTSP_RECV_DIALOG, pParent)
{

}

CRtspReceiver::~CRtspReceiver()
{
}

void CRtspReceiver::DoDataExchange(CDataExchange* pDX)
{
	DDX_Control(pDX, IDC_LT_FRAME_INFO, m_Frame_info);
	DDX_Control(pDX, IDC_EDD_SESSIONCNT, m_SessionCount);
	DDX_Control(pDX, IDC_IPADDR, m_IPControl);
	DDX_Control(pDX, IDC_EDT_UUID, m_DeviceUUID);
	DDX_Control(pDX, IDC_EDT_CSV_FILENAME, m_csv_filename);
	DDX_Control(pDX, IDC_EDT_CSV_ADDR, m_csv_addr);
	DDX_Control(pDX, IDC_EDT_BEGIN_INDEX, m_begin_index);
	DDX_Control(pDX, IDC_EDT_SERVER_PORT, m_Edt_ServerPort);
	DDX_Control(pDX, IDC_EDD_CONNECT_TIMEOUT, m_Edt_Connect_Timeout);
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CRtspReceiver, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_CONNECT, &CRtspReceiver::OnBnClickedBtnConnect)
	ON_BN_CLICKED(IDC_BTN_DisConnect, &CRtspReceiver::OnBnClickedBtnDisconnect)
	ON_MESSAGE(WM_SEND_TEST, &CRtspReceiver::test)
	ON_MESSAGE(WM_POST_TEST, &CRtspReceiver::test)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_RADIO1, IDC_RADIO2, &CRtspReceiver::OnRangeRadioGroup1)
	ON_BN_CLICKED(IDC_BTN_CSV_CONNECT, &CRtspReceiver::OnBnClickedBtnCsvConnect)
END_MESSAGE_MAP()


// CRtspReceiver 메시지 처리기


BOOL CRtspReceiver::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_Frame_info.SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
	m_Frame_info.InsertColumn(0, _T("session Index"), LVCFMT_LEFT, 100);
	m_Frame_info.InsertColumn(1, _T("profile"), LVCFMT_LEFT, 100);
	m_Frame_info.InsertColumn(2, _T("frame Index"), LVCFMT_LEFT, 100);
	m_Frame_info.InsertColumn(3, _T("frame drop"), LVCFMT_LEFT, 100);
	m_Frame_info.InsertColumn(4, _T("FPS"), LVCFMT_LEFT, 100);
	m_Frame_info.InsertColumn(5, _T("avg FPS(total frame/ total time)"), LVCFMT_LEFT, 200);
	m_Frame_info.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);

	m_SessionCount.SetWindowText(_T("1"));
	m_begin_index.SetWindowText(_T("0"));
	m_Edt_ServerPort.SetWindowText(_T("8000"));
	m_Edt_Connect_Timeout.SetWindowText(_T("5"));

	((CButton*)GetDlgItem(IDC_RADIO1))->SetCheck(TRUE);
	((CButton*)GetDlgItem(IDC_RAD_ONLY_PRIMARY))->SetCheck(TRUE);

	strPathIni = _T("info.ini");
	TCHAR strReadIni1[50] = { 0 };
	TCHAR strReadIni2[100] = { 0 };
	TCHAR strReadIni3[250] = { 0 };
	::GetPrivateProfileString(_T("SectionA"), _T("IP"), _T("127.0.0.1"), strReadIni1, 50, strPathIni);
	::GetPrivateProfileString(_T("SectionA"), _T("UUID"), _T("11"), strReadIni2, 100, strPathIni);
	::GetPrivateProfileString(_T("SectionA"), _T("CSVFile"), _T(""), strReadIni3, 250, strPathIni);

	SetDlgItemText(IDC_IPADDR, strReadIni1);
	m_DeviceUUID.SetWindowText(strReadIni2);
	m_csv_addr.SetWindowText(_T("/0/profile2/media.smp"));
	m_csv_filename.SetWindowText(strReadIni3);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 예외: OCX 속성 페이지는 FALSE를 반환해야 합니다.
}

LRESULT CRtspReceiver::test(WPARAM wparam, LPARAM lparam)
{
	UINT wp = wparam;
	LONG lp = lparam;

	Datas* data = (Datas*)lparam;
	CString strFrmNum = _T(""), strSessionIndex = _T(""), strFPS = _T(""), strstr = _T("");
	strFrmNum.Format(_T("%d"), data->nFrameNumber);
	strSessionIndex.Format(_T("%d"), data->nSessionIndex);
	CString cstr(data->m_profile_.c_str());

	m_Frame_info.SetItem(data->nSessionIndex, 0, LVIF_TEXT, strSessionIndex, NULL, NULL, NULL, NULL);
	m_Frame_info.SetItem(data->nSessionIndex, 1, LVIF_TEXT, cstr, NULL, NULL, NULL, NULL);
	m_Frame_info.SetItem(data->nSessionIndex, 2, LVIF_TEXT, strFrmNum, NULL, NULL, NULL, NULL);
	strstr.Format(_T("%d"), data->nFrmDrop);
	m_Frame_info.SetItem(data->nSessionIndex, 3, LVIF_TEXT, strstr, NULL, NULL, NULL, NULL);
	strFPS.Format(_T("%d"), data->nFps);
	m_Frame_info.SetItem(data->nSessionIndex, 4, LVIF_TEXT, strFPS, NULL, NULL, NULL, NULL);
	strFPS.Format(_T("%d"), data->nAvgFps);
	m_Frame_info.SetItem(data->nSessionIndex, 5, LVIF_TEXT, strFPS, NULL, NULL, NULL, NULL);
	if (data != NULL)
	{
		delete data;
		data = NULL;
	}
	return 0;
}

void CRtspReceiver::OnBnClickedBtnConnect()
{
	OnBnClickedBtnDisconnect();

	BOOL bDeviceIDIndex = ((CButton*)GetDlgItem(IDC_RADIO1))->GetCheck();
	BOOL bOnlyPrimary = ((CButton*)GetDlgItem(IDC_RAD_ONLY_PRIMARY))->GetCheck();
	BOOL bOnlysecondary = ((CButton*)GetDlgItem(IDC_RAD_ONLY_SECONDARY))->GetCheck();
	CString device_uuid__;
	m_DeviceUUID.GetWindowText(device_uuid__);
	std::string device_uuid = std::string(CT2CA(device_uuid__));
	BYTE ipFirst, ipSecond, ipThird, ipForth;
	CString strIPAddr;

	// BYTE 변수에 IP Address Control 값 Save 
	m_IPControl.GetAddress(ipFirst, ipSecond, ipThird, ipForth);
	std::string ip_addr;
	CString strIPAddr___;
	strIPAddr___.Format(_T("%d.%d.%d.%d"), ipFirst, ipSecond, ipThird, ipForth);
	ip_addr = std::to_string(ipFirst) + "." + std::to_string(ipSecond) + "." + std::to_string(ipThird) + "." + std::to_string(ipForth);

	CString sesison_count = _T("");
	m_SessionCount.GetWindowText(sesison_count);
	int nSession_count = _ttoi(sesison_count);

	CString begin_index = _T("");
	m_begin_index.GetWindowText(begin_index);
	int nBeginIndex = _ttoi(begin_index);

	CString str_temp = _T("");
	m_Edt_ServerPort.GetWindowText(str_temp);
	int nServer_Port = _ttoi(str_temp);

	m_Edt_Connect_Timeout.GetWindowText(str_temp);
	int nTime_Out = _ttoi(str_temp);

	m_Frame_info.DeleteAllItems();
	run_thread_ = true;

	::WritePrivateProfileString(_T("SectionA"), _T("IP"), strIPAddr___, strPathIni);
	::WritePrivateProfileString(_T("SectionA"), _T("UUID"), device_uuid__, strPathIni);

	int nDeviceID = 0;
	std::vector<std::string> profiles;
	if (bOnlyPrimary) {
		profiles.push_back("primary");
	}
	else if (bOnlysecondary) {
		profiles.push_back("secondary");
	}
	else {
		profiles.push_back("primary");
		profiles.push_back("secondary");
	}
	for (int i = nBeginIndex; i < nSession_count + nBeginIndex; i++)
	{
		for (auto profile : profiles) {
			CString index_number = _T("");
			index_number.Format(_T("%d"), nDeviceID);
			m_Frame_info.InsertItem(nDeviceID, index_number);
			std::shared_ptr<RTSPConnector> rtsp_tester = std::make_shared<RTSPConnector>(nDeviceID, GetSafeHwnd());
			std::thread* th = new std::thread([rtsp_tester, this, i, ip_addr, bDeviceIDIndex, device_uuid, profile, nServer_Port, nTime_Out]() {
				std::string device_id = "";
				if (bDeviceIDIndex) {
					device_id = std::to_string(i);
				}
				else {
					device_id = device_uuid;
				}
				rtsp_tester->ServerConnect(ip_addr, nServer_Port, device_id, profile, nTime_Out);
				return 0;
				});


			rtsp_treads_.push_back(th);
			rtsp_testers.push_back(rtsp_tester);
			nDeviceID++;
		}
	}

	OutputDebugString(_T("접속 완료"));
}

void CRtspReceiver::OnBnClickedBtnDisconnect()
{
	run_thread_ = false;
	std::vector<std::thread*> treads;
	for (auto rtsp_tester : rtsp_testers) {
		rtsp_tester->Close();
	}
	rtsp_testers.clear();

	for (auto th : rtsp_treads_) {
		th->join();
		if (th) {
			delete th;
		}
	}
	rtsp_treads_.clear();

	MessageBox(_T("RTSP Close 완료"));
}

void CRtspReceiver::OnBnClickedBtnCsvConnect()
{
	std::mutex mutex_;
	OnBnClickedBtnDisconnect();
	CString str_csv_filename;
	m_csv_filename.GetWindowText(str_csv_filename);

	CString str_csv_addr;
	m_csv_addr.GetWindowText(str_csv_addr);

	CString str_temp = _T("");

	m_Edt_Connect_Timeout.GetWindowText(str_temp);
	int nTime_Out = _ttoi(str_temp);


	if (str_csv_filename.GetLength() == 0 || str_csv_addr.GetLength() == 0)
	{
		MessageBox(_T("파일명 or csv 파일 정보가 없습니다."));
		return;
	}
	::WritePrivateProfileString(_T("SectionA"), _T("CSVFile"), str_csv_filename, strPathIni);
	std::vector<std::string> addrs = GetRTSPaddrFromCSV(std::string(CT2CA(str_csv_filename)), std::string(CT2CA(str_csv_addr)));

	m_Frame_info.DeleteAllItems();
	run_thread_ = true;

	int nDeviceID = 0;
	for (auto addr : addrs) {
		CString index_number = _T("");
		index_number.Format(_T("%d"), nDeviceID);
		m_Frame_info.InsertItem(nDeviceID, index_number);
		std::shared_ptr<RTSPConnector> rtsp_tester = std::make_shared<RTSPConnector>(nDeviceID, GetSafeHwnd());
		std::thread* th = new std::thread([rtsp_tester, this, nDeviceID, addr, nTime_Out]() {

			rtsp_tester->ServerConnectFromAddr(addr, nTime_Out);
			return 0;
			});


		rtsp_treads_.push_back(th);
		rtsp_testers.push_back(rtsp_tester);
		nDeviceID++;
	}

	OutputDebugString("접속 완료");
}



std::vector<std::vector<std::shared_ptr<RTSPConnector>>> CRtspReceiver::SplitVector(
	const std::vector<std::shared_ptr<RTSPConnector>>& inputVector, size_t threadSize) {

	std::vector<std::vector<std::shared_ptr<RTSPConnector>>> resultVector;
	size_t totalSize = inputVector.size();
	size_t numBatches = totalSize / threadSize + (totalSize % threadSize);

	int nIndex = 0;
	std::vector<std::shared_ptr<RTSPConnector>> tp_Connectors;
	for (auto connector : inputVector) {
		if (nIndex % numBatches == 0 && nIndex != 0)
		{
			resultVector.push_back(tp_Connectors);
			tp_Connectors.clear();
		}
		tp_Connectors.push_back(connector);
		nIndex++;
	}
	if (tp_Connectors.size() != 0) {
		resultVector.push_back(tp_Connectors);
		tp_Connectors.clear();
	}

	return resultVector;
}



std::vector<std::string>  CRtspReceiver::GetRTSPaddrFromCSV(std::string str_csv_filename, std::string str_csv_addr)
{
	std::vector<std::string> addrs;
	std::ifstream file(str_csv_filename);
	if (file.is_open()) {
		std::string line;
		while (std::getline(file, line)) {
			std::istringstream iss(line);
			std::string ip;
			std::string portStr;

			if (std::getline(iss, ip, ',') && std::getline(iss, portStr, ',')) {
				try {
					std::string addr = "rtsp://admin:000ppp[[[@";
					addr += ip + ":" + portStr + str_csv_addr;
					addrs.push_back(addr);
				}
				catch (const std::exception& ex) {
					std::cerr << "Invalid port number: " << portStr << std::endl;
				}
			}
		}
		file.close();
	}
	else {
	}
	return addrs;
}


void CRtspReceiver::OnRangeRadioGroup1(UINT uID)
{
	switch (uID)
	{
	case IDC_RADIO1:
		break;
	case IDC_RADIO2:
		break;
	}
}