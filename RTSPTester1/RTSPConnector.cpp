#include "pch.h"
#include "RTSPConnector.h"
#include "define.h"
#include <fstream>

#include <chrono>
#include <iostream>
#include <ratio>
#include <thread>
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

RTSPConnector::RTSPConnector(int nSessionIndex, HWND Hwnd) :Hwnd_(Hwnd), nSessionIndex_(nSessionIndex)
{
	m_rtspClient = new RTSPClient();
	m_rtspClient->m_pParent = this;
	m_nTotalFrmCount = 0;
	m_nPrevFrmNum = 0;
	m_nFrmDrop = 0;
}

RTSPConnector::~RTSPConnector()
{
	delete m_rtspClient; 
}



static void frameHandlerFunc(void* arg, RTP_FRAME_TYPE frame_type, int64_t timestamp, unsigned char* buf, int len, long long nReplayFramNum)
{
	RTSPClient* rtspClient = (RTSPClient*)arg;
	RTSPConnector* pThis = (RTSPConnector*)rtspClient->m_pParent;

	if (pThis != nullptr)
		pThis->frameHandlerFunc_(frame_type, timestamp, buf, len, nReplayFramNum);
}

void RTSPConnector::WriteFile(unsigned char* buf, int len)
{
	//std::string filename = "movie.h264";

	//std::ofstream file(filename, std::ios::binary | std::ios::app);
	//if (!file) {
	//	return ;
	//}
	//file.write((const char*)buf, len);
	//file.close();
}


int index = 0;
int RTSPConnector::frameHandlerFunc_(RTP_FRAME_TYPE frame_type, int64_t timestamp, unsigned char* buf, int len, int nReplayFramNum)
{
	bool bModify_sec = false;
	if (frame_type != FRAME_TYPE_VIDEO)
		return 0;

	uint64_t sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	if(nPrevSec != sec)
	{
		nFps = nFpsCount;
		nFpsCount = 1;
		nPrevSec = sec;
		bModify_sec = true;
	}
	else {
		nFpsCount++;
	}

	Datas* data = new Datas();
	data->nFrameNumber = timestamp;
	data->nSessionIndex = nSessionIndex_;
	data->nFps = nFps;
	data->m_profile_ = m_profile_;
	m_nTotalFrmCount++;

	int nfrmGap = nReplayFramNum - m_nPrevFrmNum;
	if (nfrmGap != 1) {
		m_nFrmDrop += (nfrmGap - 1);
	}
	data->nFrmDrop = m_nFrmDrop;
	m_nPrevFrmNum = nReplayFramNum;

	auto duration_total = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_first_get_frame);
	if (m_nTotalFrmCount > 30)
	{
		data->nAvgFps = ((double)m_nTotalFrmCount / duration_total.count()) * 1000;
	}

	if(bModify_sec)
		LONG result = PostMessage(Hwnd_, WM_SEND_TEST, 0, (LPARAM)data);
	else
	{
		if (data)
			delete data;
	}

	if (!is_recv)
	{
		m_first_get_frame = std::chrono::steady_clock::now();
		auto end = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		std::cout << "Count "<< nCount++ << ", Elapsed time: [" << duration.count() << "] milliseconds" << std::endl;
		is_recv = true;
	}


	//int nH264size = 4, nH265size = 4;
	//unsigned char* found_sei_264 = NULL, * found_sei_265 = NULL;
	//found_sei_264 = (unsigned char*)rmemmem(buf, 520, PAYLOAD_SEI_PACKET_H264_IFRAME, nH264size);
	//if (found_sei_264 != NULL)      
	//{
	//	std::cout << "H264 I timestamp [" << timestamp << "]" << index << std::endl;
	//	index = 0;
	//}
	//found_sei_264 = (unsigned char*)rmemmem(buf, 520, PAYLOAD_SEI_PACKET_H264_PFRAME, nH264size);
	//if (found_sei_264 != NULL)
	//{
	//	//std::cout << "H264 P timestamp [" << timestamp << "]" << std::endl;
	//	index++;
	//}

	//found_sei_265 = (unsigned char*)rmemmem(buf, 520, PAYLOAD_SEI_PACKET_H265_IFRAME, nH264size);
	//if (found_sei_265 != NULL)
	//{
	//	std::cout << "H265 I timestamp [" << timestamp << "]" << index << std::endl;
	//	index = 0;
	//}
	//found_sei_265 = (unsigned char*)rmemmem(buf, 520, PAYLOAD_SEI_PACKET_H265_PFRAME, nH264size);
	//if (found_sei_265 != NULL)
	//{
	//	//std::cout << "H265 P timestamp [" << timestamp << "]" << index << std::endl;
	//	index++;
	//}
	//std::cout << "I timestamp [" << timestamp << "]" << index << std::endl;
	//WriteFile(buf, len);


	return 0;
}

int RTSPConnector::QuickPlay(std::string strIP, std::string device_id)
{
	start = std::chrono::steady_clock::now();
	std::string mSrcUrl;
	mSrcUrl += "rtsp://";
	mSrcUrl += strIP;
	mSrcUrl += ":8001/";
	mSrcUrl += device_id;
	mSrcUrl += "/primary";
	int nRtspTimeOUt = 20;
	if (m_rtspClient->openQuickURL(mSrcUrl.c_str(), 1, nRtspTimeOUt) == 0)
	{
		if (m_rtspClient->playQuikURL((FrameHandlerFunc)frameHandlerFunc, m_rtspClient, NULL, NULL) == 0)
		{
			//std::cout << "success" << std::endl;

		}
		else
		{
			std::cout << "PRESD_ERR_START_AGENT" << std::endl;
		}

	}
	else
	{
		std::cout << "RTSP Open Error" << std::endl;
	}
	return 1;
}

int RTSPConnector::ServerConnect(std::string strIP, int rtsp_port, std::string device_id, std::string profile, int timeout)
{
	start = std::chrono::steady_clock::now();
	std::string mSrcUrl;
	mSrcUrl += "rtsp://";
	mSrcUrl += strIP;
	mSrcUrl += ":8000/";
	mSrcUrl += device_id;
	mSrcUrl += "/";
	mSrcUrl += profile;
	m_profile_ = profile;
	mSrcUrl = "rtsp://192.168.171.233:554/0/recording/backup.smp";
	if (m_rtspClient->openURL(mSrcUrl.c_str(), 1, timeout) == 0)
	{
		if (m_rtspClient->playURL((FrameHandlerFunc)frameHandlerFunc, m_rtspClient, NULL, NULL) == 0)
		{
			//std::cout << "success" << std::endl;

		}
		else
		{
			std::cout << "PRESD_ERR_START_AGENT" << std::endl;
		}

	}
	else
	{
		std::cout << "RTSP Open Error" << std::endl;
	}
	return 1;
}


int RTSPConnector::ServerConnectFromAddr(std::string addr, int timeout)
{
	std::string mSrcUrl = addr;

	if (m_rtspClient->openURL(mSrcUrl.c_str(), 1, timeout) == 0)
	{
		if (m_rtspClient->playURL((FrameHandlerFunc)frameHandlerFunc, m_rtspClient, NULL, NULL) == 0)
		{
			std::cout << "success" << std::endl;
		}
		else
		{
			std::cout << "PRESD_ERR_START_AGENT" << std::endl;
		}

	}
	else
	{
		std::cout << "RTSP Open Error" << std::endl;
	}
	return 1;
}

int RTSPConnector::Palyback(std::string strIP, std::string device_id, std::string start_time, std::string end_time, float scale) {
	start = std::chrono::steady_clock::now();
	std::string mSrcUrl;
	mSrcUrl += "rtsp://";
	mSrcUrl += strIP;
	mSrcUrl += ":8001/";
	mSrcUrl += device_id;
	mSrcUrl += "/primary";
	int nRtspTimeOUt = 20;
	if (m_rtspClient->openURL(mSrcUrl.c_str(), 1, nRtspTimeOUt, false, true) == 0)
	{
		if (m_rtspClient->playURL2((FrameHandlerFunc)frameHandlerFunc, m_rtspClient, NULL, NULL, NULL, NULL, NULL, NULL,
			start_time, end_time, scale) == 0)
		{
			//std::cout << "success" << std::endl;

		}
		else
		{
			std::cout << "PRESD_ERR_START_AGENT" << std::endl;
		}

	}
	else
	{
		std::cout << "RTSP Open Error" << std::endl;
	}
	return 1;
}

int RTSPConnector::ServerConnectPlayBack(std::string strIP, std::string device_id, std::string timestamp)
{
	start = std::chrono::steady_clock::now();
	std::string mSrcUrl;
	mSrcUrl += "rtsp://";
	mSrcUrl += strIP;
	mSrcUrl += ":8001/";
	mSrcUrl += device_id;
	mSrcUrl += "/primary/";
	mSrcUrl += timestamp;
	int nRtspTimeOUt = 20;
	if (m_rtspClient->openURL(mSrcUrl.c_str(), 1, nRtspTimeOUt) == 0)
	{
		if (m_rtspClient->playURL((FrameHandlerFunc)frameHandlerFunc, m_rtspClient, NULL, NULL) == 0)
		{
			//std::cout << "success" << std::endl;

		}
		else
		{
			std::cout << "PRESD_ERR_START_AGENT" << std::endl;
		}

	}
	else
	{
		std::cout << "RTSP Open Error" << std::endl;
	}
	return 1;
}


int RTSPConnector::CamConnect(std::string strIP, std::string id, std::string passwd)
{
	start = std::chrono::steady_clock::now();
	std::string mSrcUrl;
	mSrcUrl += "rtsp://";
	mSrcUrl += id;
	mSrcUrl += ":";
	mSrcUrl += passwd;
	mSrcUrl += "@";
	mSrcUrl += strIP;
	mSrcUrl += "/profile5/media.smp";
	int nRtspTimeOUt = 20;
	if (m_rtspClient->openURL(mSrcUrl.c_str(), 1, nRtspTimeOUt) == 0)
	{
		if (m_rtspClient->playURL((FrameHandlerFunc)frameHandlerFunc, m_rtspClient, NULL, NULL) == 0)
		{
			//std::cout << "success" << std::endl;

		}
		else
		{
			std::cout << "PRESD_ERR_START_AGENT" << std::endl;
		}

	}
	else
	{
		std::cout << "RTSP Open Error" << std::endl;
	}
	return 1;
}


void RTSPConnector::Close()
{
	m_rtspClient->closeURL();
	is_recv = false;
}
