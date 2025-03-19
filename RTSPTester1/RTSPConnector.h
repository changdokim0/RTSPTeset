#pragma once
#include <iostream>
#include <chrono>
#include "RTSPClient.h"
#include "Resource.h"

static unsigned char PAYLOAD_SEI_PACKET_H264_IFRAME[] = { 0x00, 0x00, 0x01, 0x65 };
static unsigned char PAYLOAD_SEI_PACKET_H264_PFRAME[] = { 0x00, 0x00, 0x01, 0x01 };

static unsigned char PAYLOAD_SEI_PACKET_H265_IFRAME[] = { 0x00, 0x00, 0x01, 0x28, 0x01 };
static unsigned char PAYLOAD_SEI_PACKET_H265_PFRAME[] = { 0x00, 0x00, 0x01, 0x02, 0x01 };
class RTSPConnector
{
public:
	RTSPConnector(int nSessionIndex, HWND Hwnd);
	~RTSPConnector();

	int ServerConnect(std::string strIP, int rtsp_port, std::string device_id, std::string profile, int timeout);
	int ServerConnectFromAddr(std::string addr, int timeout);
	int QuickPlay(std::string strIP, std::string device_id);
	int ServerConnectPlayBack(std::string strIP, std::string device_id, std::string timestamp);
	void Close();
	int CamConnect(std::string strIP, std::string id, std::string passwd);
	int frameHandlerFunc_(RTP_FRAME_TYPE frame_type, int64_t timestamp, unsigned char* buf, int len, int nReplayFramNum);
	void WriteFile(unsigned char* buf, int len);
	int Palyback(std::string strIP, std::string device_id, std::string start_time = "", std::string end_time = "", float scale = 1.0f);
	//void eventRoop();
private:

	RTSPClient* m_rtspClient;
	bool is_recv = false;
	std::chrono::time_point<std::chrono::steady_clock> start, m_first_get_frame;
	int nCount = 0;
	HWND Hwnd_;
	int nSessionIndex_ = 0;
	int nFps = 0, nFpsCount = 0;;
	int nPrevSec = 0;
	int m_nTotalFrmCount = 0;
	int m_nPrevFrmNum = 0;
	int m_nFrmDrop = 0;
	std::string m_profile_ = "";
};

