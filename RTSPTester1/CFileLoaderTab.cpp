// CFileLoaderTab.cpp: 구현 파일
//

#include "pch.h"
#include "RTSPTester1.h"
#include "afxdialogex.h"
#include "CFileLoaderTab.h"
#include <archive_define.h>

// CFileLoaderTab 대화 상자
IMPLEMENT_DYNAMIC(CFileLoaderTab, CDialogEx)
CFileLoaderTab::CFileLoaderTab(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DLG_FILELOADER_DIALOG, pParent)
{
	strPathIni = _T("info.ini");
	InitSeeker();
}

CFileLoaderTab::~CFileLoaderTab()
{
}

void CFileLoaderTab::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDT_FILELOADER_SELPATH, m_FileSelectPath);
	DDX_Control(pDX, IDC_EDIT_FILELOAD_SEEK, m_FileSeekTimeEdit);
	DDX_Control(pDX, IDC_LIST_FILELOADER, m_listCtrl);
	DDX_Control(pDX, IDC_STC_VIEWER, m_videoWnd);
	DDX_Control(pDX, IDC_EDIT2, m_edt_box);
}

BEGIN_MESSAGE_MAP(CFileLoaderTab, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_FILELOADER_FILEOPEN, &CFileLoaderTab::OnBnClickedBtnFileloaderFileopen)
	ON_BN_CLICKED(IDC_BTN_FILELOAD_SEEK, &CFileLoaderTab::OnBnClickedBtnFileloadSeek)
	ON_BN_CLICKED(IDC_BUTTON1, &CFileLoaderTab::OnBnClickedButton1)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FILELOADER, &CFileLoaderTab::OnListItemChanged)
	ON_BN_CLICKED(IDC_BTN_FILELOAD_NEXT, &CFileLoaderTab::OnBnClickedBtnFileloadNext)
	ON_BN_CLICKED(IDC_BTN_FILELOAD_GETGOP, &CFileLoaderTab::OnBnClickedBtnFileloadGetgop)
END_MESSAGE_MAP()


// CFileLoaderTab 메시지 처리기

BOOL CFileLoaderTab::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	HTREEITEM hChild1, hChild2;

	m_listCtrl.SetExtendedStyle(LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
	m_listCtrl.InsertColumn(LIST_ITEM_INDEX, _T("Index"), LVCFMT_LEFT, 40);
	m_listCtrl.InsertColumn(LIST_ITEM_TYPE, _T("Type"), LVCFMT_LEFT, 50);
	m_listCtrl.InsertColumn(LIST_ITEM_CODECTYPE, _T("CodecType"), LVCFMT_LEFT, 50);
	m_listCtrl.InsertColumn(LIST_ITEM_FRAMETYPE, _T("FrameType"), LVCFMT_LEFT, 70);
	m_listCtrl.InsertColumn(LIST_ITEM_FRMAESIZE, _T("size"), LVCFMT_LEFT, 80);
	m_listCtrl.InsertColumn(LIST_ITEM_PACKETSIZE, _T("packetsize"), LVCFMT_LEFT, 60);
	m_listCtrl.InsertColumn(LIST_ITEM_EPOCHTIME, _T("epoch time"), LVCFMT_LEFT, 100);
	m_listCtrl.InsertColumn(LIST_ITEM_TIME, _T("time"), LVCFMT_LEFT, 160);
	m_listCtrl.InsertColumn(LIST_ITEM_LOCALTIME, _T("Local time"), LVCFMT_LEFT, 160);

	ffmpegWrapper.Initialize(CODEC_TYPE::H264);
	m_FileSeekTimeEdit.SetWindowText("1760837129394");

	return TRUE;  // return TRUE unless you set the focus to a control
}

void CFileLoaderTab::OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	// Check if the item is selected
	if ((pNMLV->uNewState & LVIS_SELECTED) && !(pNMLV->uOldState & LVIS_SELECTED))
	{
		HandleListItemSelection();
	}
}


void CFileLoaderTab::HandleListItemSelection()
{
	int selectedItem = m_listCtrl.GetNextItem(-1, LVNI_SELECTED);
	bool search = false;
	m_edt_box.SetWindowText(_T(""));
	CODEC_TYPE codec_type;
	if (selectedItem != -1)
	{
		YUVData yuvData; // YUVData 구조체
		CString epochTimeStr = m_listCtrl.GetItemText(selectedItem, LIST_ITEM_EPOCHTIME);
		CString datatypeStr = m_listCtrl.GetItemText(selectedItem, LIST_ITEM_TYPE);
		long long epochTime = _ttoi64(epochTimeStr);

		if (datatypeStr.Compare("Meta") == 0) {
			for (int ind = 0; ind < buffers_all_.size(); ind++) {
				auto buffers = buffers_all_[ind];
				if (buffers.has_value())
				{
					for (auto item : *buffers)
					{
						if (item->timestamp_msec == epochTime && item->archive_type == kArchiveTypeMeta) {
							if (auto meta = std::dynamic_pointer_cast<MetaData>(item)) {
								CString strMeta;// (meta->buffer->data());
								strMeta.Format(_T("%.*s"), meta->buffer->dataSize(), meta->buffer->data());
								m_edt_box.SetWindowText(_T(""));
								m_edt_box.SetWindowText(strMeta);
								return;
							}
						}
					}
				}
			}
		}

		if (datatypeStr.Compare("Video") != 0)
			return;

		int bufferIndex = _ttoi(m_listCtrl.GetItemText(selectedItem, 0));
		if (bufferIndex >= 0 )
		{
			int search_idx = 0;
			for (int ind = 0; ind < buffers_all_.size(); ind++) {
				auto buffers = buffers_all_[ind];
				if (buffers.has_value()){
					if (buffers->at(0)->timestamp_msec > epochTime)
						break;

					if (auto video = std::dynamic_pointer_cast<VideoData>(buffers->at(0))) {
						if (video->archive_header.codecType == Pnx::Media::VideoCodecType::H265)
							codec_type = CODEC_TYPE::H265;
						else if (video->archive_header.codecType == Pnx::Media::VideoCodecType::H264)
							codec_type = CODEC_TYPE::H264;
						else
							codec_type = CODEC_TYPE::Unknown;

						if (video->archive_header.frameType == Pnx::Media::VideoFrameType::I_FRAME) {
							search_idx = ind;
						}
					}

				}
			}

			for (int ind = search_idx; ind < buffers_all_.size(); ind++) {
				auto buffers = buffers_all_[ind];
				if (buffers.has_value())
				{
					for (auto item : *buffers)
					{
						if (item->archive_type == kArchiveTypeFrameVideo)
						{
							if (ffmpegWrapper.GetCodecType() != codec_type) {
								ffmpegWrapper.Cleanup();
								ffmpegWrapper.Initialize(codec_type);
							}
							ffmpegWrapper.ReceiveFrame(item->buffer->data(), item->buffer->dataSize(), yuvData);
							if (item->timestamp_msec == epochTime) {
								search = true;
								break;
							}
						}
					}
				}
				if (search)
					break;
			}
		}
		ffmpegWrapper.GetFrame(yuvData);
		m_videoWnd.LoadVideoFrame(yuvData.yData.get(), yuvData.uData.get(), yuvData.vData.get(), yuvData.width, yuvData.height);
	}
}

void CFileLoaderTab::OnBnClickedBtnFileloaderFileopen()
{
	CFileDialog fileDlg(TRUE, _T("dat"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, _T("Phoenix Files (*.dat;*.dat2)|*.dat;*.dat2|All Files (*.*)|*.*||"));
	TCHAR strReadIni1[150] = { 0 };
	::GetPrivateProfileString(_T("FILE_LOADER"), _T("LOAD_PATH"), _T(""), strReadIni1, 50, strPathIni);
	fileDlg.m_ofn.lpstrInitialDir = strReadIni1;
	if (fileDlg.DoModal() == IDOK)
	{
		CString filePath = fileDlg.GetPathName();
		CString fileFolderPaht = fileDlg.GetFolderPath();
		::WritePrivateProfileString(_T("FILE_LOADER"), _T("LOAD_PATH"), fileFolderPaht, strPathIni);
		m_FileSelectPath.SetWindowText(filePath);
		OpenFileLoad(filePath);
	}
}


void CFileLoaderTab::OpenFileLoad(CString filePath) 
{
	CStringA filePathA(filePath);
	std::string stdFilePath(filePathA.GetString());
	buffers_all_.clear();
	read_object_ = std::make_shared<ReaderObject>(MediaProfile::kSecondary);
	read_object_->FileOpen(stdFilePath);
	read_object_.get()->ParseMainHeader();
	read_object_.get()->LoadDataByIndex(0, ArchiveReadType::kArchiveReadNext);
	while (true) {
		std::optional<std::vector<std::shared_ptr<StreamBuffer>>> buffers = read_object_.get()->GetNextData(ArchiveType::kArchiveTypeFrameVideo);
		if (buffers != std::nullopt) {
			buffers_all_.push_back(buffers);
		}
		else {
			//LOG(AL_INFO, "");
			break;
		}
		//break;
	}
	MakeTree();
}

void CFileLoaderTab::DeleteAllChildItems(CTreeCtrl& treeCtrl)
{
	if (tree_root_ != nullptr)
	{
		HTREEITEM hChild = treeCtrl.GetChildItem(tree_root_);
		while (hChild != nullptr)
		{
			HTREEITEM hNext = treeCtrl.GetNextItem(hChild, TVGN_NEXT);
			treeCtrl.DeleteItem(hChild);
			hChild = hNext;
		}
	}
}

CString CFileLoaderTab::GetTypeString(int type) {
	switch (type) {
	case kArchiveTypeNone:
		return "NONE";
	case kArchiveTypeGopVideo:
		return "GopVideo";
	case kArchiveTypeFrameVideo:
		return "Video";
	case kArchiveTypeAudio:
		return "Audio";
	case kArchiveTypeMeta:
		return "Meta";
	}


}
void CFileLoaderTab::MakeTree() {
	m_listCtrl.DeleteAllItems();
	int nIndex = 0, list_index = 0;
	long long prev_time_stamp = 0;
	CString strItemText;
	m_listCtrl.SetRedraw(FALSE);
	int nframesize = 0;
	for (auto buffers: buffers_all_) {
		strItemText.Format(_T("%d"), nIndex++);
		//HTREEITEM hChild = m_FileLoaderTreeCtrl.InsertItem(strItemText, tree_root_, TVI_LAST);
		if (buffers.has_value() == false)
			continue;

		CString strFrameSize = _T(""), strPacketSize = _T("");
		for (auto buffer : *buffers) {
			CString type = GetTypeString(buffer->archive_type);
			CString frame_Type = _T("");
			CString codec_Type = _T("");
			if (auto video = std::dynamic_pointer_cast<VideoData>(buffer)) {
				nframesize++;
				if(video->archive_header.codecType == Pnx::Media::VideoCodecType::H265)
					codec_Type = _T("H265");
				else if (video->archive_header.codecType == Pnx::Media::VideoCodecType::H264)
					codec_Type = _T("H264");
				else
					codec_Type = _T("Unknown");

				if (video->archive_header.frameType == Pnx::Media::VideoFrameType::I_FRAME) {
					nframesize = 0;
					frame_Type = _T("I_FRAME");
				}
				else if (video->archive_header.frameType == Pnx::Media::VideoFrameType::P_FRAME)
					frame_Type = _T("P_FRAME");
				else
					frame_Type = _T("Unknown");
				strFrameSize.Format(_T("%dx%d"), video->archive_header.width, video->archive_header.height);


				long long gap_timestamp = buffer->timestamp_msec - prev_time_stamp;
				std::cout << "gap_timestamp: " << gap_timestamp << std::endl;
				prev_time_stamp = buffer->timestamp_msec;

			}
			strPacketSize.Format(_T("%d"), buffer->packet_size);
			CString buffer_info;
			buffer_info.Format("%s_%s_%s", type, codec_Type, frame_Type);

			CString strListIndex, epochTime, Time, localTime;
			strListIndex.Format(_T("%d"), list_index++);
			epochTime.Format(_T("%I64d"), buffer->timestamp_msec);

			ConvertEpochToGMTAndLocalCString(buffer->timestamp_msec, Time, localTime);

			int list_col_Index = m_listCtrl.InsertItem(list_index, strListIndex);
			m_listCtrl.SetItemText(list_col_Index, LIST_ITEM_TYPE,  type);
			m_listCtrl.SetItemText(list_col_Index, LIST_ITEM_CODECTYPE, codec_Type);
			m_listCtrl.SetItemText(list_col_Index, LIST_ITEM_FRAMETYPE, frame_Type);
			m_listCtrl.SetItemText(list_col_Index, LIST_ITEM_FRMAESIZE, strFrameSize);
			m_listCtrl.SetItemText(list_col_Index, LIST_ITEM_PACKETSIZE, strPacketSize);
			m_listCtrl.SetItemText(list_col_Index, LIST_ITEM_EPOCHTIME, epochTime);
			m_listCtrl.SetItemText(list_col_Index, LIST_ITEM_TIME, Time);
			m_listCtrl.SetItemText(list_col_Index, LIST_ITEM_LOCALTIME, localTime);
		}
	}
	m_listCtrl.SetRedraw(TRUE);
}

void CFileLoaderTab::ConvertEpochToGMTAndLocalCString(long long epochTime, CString& gmtStr, CString& localStr)
{
	// epochTime을 seconds로 변환 (밀리세컨드 제거)
	time_t seconds = static_cast<time_t>(epochTime / 1000);

	// 밀리세컨드를 계산
	int milliseconds = static_cast<int>(epochTime % 1000);

	// GMT 시간을 struct tm으로 변환
	struct tm gmtTimeInfo;
	gmtime_s(&gmtTimeInfo , &seconds);

	// 로컬 시간으로 변환
	struct tm localTimeInfo;
	localtime_s(&localTimeInfo, &seconds); // 안전한 로컬 시간 변환

	// GMT CString에 포맷팅하여 넣기
	gmtStr.Format(_T("%04d/%02d/%02d %02d:%02d:%02d - %03d"),
		gmtTimeInfo.tm_year + 1900, // tm_year는 1900년 기준
		gmtTimeInfo.tm_mon + 1,     // tm_mon은 0~11 범위
		gmtTimeInfo.tm_mday,
		gmtTimeInfo.tm_hour,
		gmtTimeInfo.tm_min,
		gmtTimeInfo.tm_sec,
		milliseconds); // 밀리세컨드 추가

	// Local CString에 포맷팅하여 넣기
	localStr.Format(_T("%04d/%02d/%02d %02d:%02d:%02d - %03d"),
		localTimeInfo.tm_year + 1900, // tm_year는 1900년 기준
		localTimeInfo.tm_mon + 1,     // tm_mon은 0~11 범위
		localTimeInfo.tm_mday,
		localTimeInfo.tm_hour,
		localTimeInfo.tm_min,
		localTimeInfo.tm_sec,
		milliseconds); // 밀리세컨드 추가
}

std::string CFileLoaderTab::formatTimestamp(long long timestampMs) {
	std::chrono::milliseconds ms(timestampMs);
	auto timePoint = std::chrono::time_point<std::chrono::system_clock>(ms);
	std::time_t timeT = std::chrono::system_clock::to_time_t(timePoint);
	std::tm localTime;
	localtime_s(&localTime, &timeT);
	auto milliseconds = ms.count() % 1000;
	std::ostringstream oss;
	oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << '.'
		<< std::setw(3) << std::setfill('0') << milliseconds;

	return oss.str(); // 형식화된 문자열 반환
}

void CFileLoaderTab::OnBnClickedButton1()
{
	//return;

	//CString strSeekTime;
	//m_FileSeekTimeEdit.GetWindowText(strSeekTime);
	//long long neekTime = _ttoll(strSeekTime);
	//read_object_.get()->LoadDataByIndex(neekTime, ArchiveReadType::kArchiveReadNext);

	//std::shared_ptr<ArchiveChunkBuffer> data = read_object_.get()->GetStreamGop(ArchiveChunkReadType::kArchiveChunkReadTarget);

	//std::optional<std::vector<std::shared_ptr<StreamBuffer>>> buffers = read_object_.get()->GetNextData(ArchiveType::kArchiveTypeFrameVideo);
	reader_worker_.SetData("/Hanwha Vision/BLAZE server/Archive/257fa333-0b4d-4e2a-8a14-3db873078497/c0908d4b-e331-4688-b5fe-328eb7804dc1");
	//reader_worker_.AddDrive("C:");
	reader_worker_.AddDrive("D:");
}


BOOL CFileLoaderTab::PreTranslateMessage(MSG* pMsg) {
	if (pMsg->wParam == VK_RETURN)
	{
		return TRUE;
	}
	if (pMsg->message == WM_KEYDOWN) {
		// Ctrl+C 처리
		if (pMsg->wParam == 'C' && GetKeyState(VK_CONTROL) < 0) {
			if (GetFocus() == &m_listCtrl) {
				CopyToClipboard(); // CListCtrl에서 복사
				return TRUE; // 키 이벤트 처리 완료
			}
		}
		// Ctrl+V 처리
		else if (pMsg->wParam == 'V' && GetKeyState(VK_CONTROL) < 0) {
			if (GetFocus() == &m_listCtrl) {
				PasteFromClipboard(); // CListCtrl에서 붙여넣기
				return TRUE; // 키 이벤트 처리 완료
			}
		}
	}
	return CDialogEx::PreTranslateMessage(pMsg); // 기본 처리
}

void CFileLoaderTab::CopyToClipboard() {
    if (m_listCtrl.GetSafeHwnd() != ::GetFocus()) {
        return; // 활성화된 컨트롤이 CListCtrl이 아닐 경우 반환
    }

    CString itemText;
    // 여러 행 선택 지원
    int nItem = -1;
    bool firstLine = true;
    while ((nItem = m_listCtrl.GetNextItem(nItem, LVNI_SELECTED)) != -1) {
        if (!firstLine) {
            itemText += _T("\r\n"); // 다음 행은 줄바꿈으로 구분
        }
        firstLine = false;
        for (int i = 0; i <= LIST_ITEM_LOCALTIME; i++) {
            itemText += m_listCtrl.GetItemText(nItem, i);
            if (i < LIST_ITEM_LOCALTIME) {
                itemText += _T("\t"); // 컬럼은 탭으로 구분
            }
        }
    }

    // 클립보드에 복사
    if (!itemText.IsEmpty() && OpenClipboard()) {
        EmptyClipboard();
#ifdef UNICODE
        HGLOBAL hGlob = GlobalAlloc(GMEM_FIXED, (itemText.GetLength() + 1) * sizeof(wchar_t));
        if (hGlob) {
            memcpy(hGlob, itemText.GetString(), (itemText.GetLength() + 1) * sizeof(wchar_t));
            SetClipboardData(CF_UNICODETEXT, hGlob);
        }
#else
        HGLOBAL hGlob = GlobalAlloc(GMEM_FIXED, (itemText.GetLength() + 1) * sizeof(char));
        if (hGlob) {
            memcpy(hGlob, itemText.GetString(), (itemText.GetLength() + 1) * sizeof(char));
            SetClipboardData(CF_TEXT, hGlob);
        }
#endif
        CloseClipboard();
    }
}

void CFileLoaderTab::PasteFromClipboard() {
	if (m_edt_box.GetSafeHwnd() != ::GetFocus()) {
		return; // 활성화된 컨트롤이 CListCtrl이 아닐 경우 반환
	}
	if (OpenClipboard()) {
		HGLOBAL hGlob = GetClipboardData(CF_TEXT);
		if (hGlob) {
			TCHAR* pData = (TCHAR*)GlobalLock(hGlob);
			if (pData) {
				CString itemText(pData);
				int newItemIndex = m_listCtrl.InsertItem(m_listCtrl.GetItemCount(), itemText);
				m_listCtrl.SetItemText(newItemIndex, 0, itemText);
				GlobalUnlock(hGlob);
			}
		}
		CloseClipboard();
	}
}



///////////////////////////////////
// Seek 관련
///////////////////////////////////

void CFileLoaderTab::OnBnClickedBtnFileloadSeek()
{
	CString strSeekTime;
	m_FileSeekTimeEdit.GetWindowText(strSeekTime);
	long long seekTime = _ttoll(strSeekTime);
	uint32_t sec = seekTime / 1000;
	uint32_t usec = uint32_t((seekTime % 1000) * 1000);
	PnxMediaTime pnxSeekTime(sec, usec);
	read_object_ = std::make_shared<ReaderObject>(MediaProfile::kPrimary);
	Seeker("2524ebae-0174-45e4-9ed6-bf4fdeb7c0a6", pnxSeekTime, kArchiveReadNext, read_object_);

	return;
}

void CFileLoaderTab::OnBnClickedBtnFileloadGetgop()
{
	std::shared_ptr<ArchiveChunkBuffer> group = GetStreamGop(kArchiveChunkReadTarget, read_object_);
	if (group == nullptr) {
		return;
	}
	auto ret = std::make_shared<PnxMediaDataGroup>();
	bool has_marked_cflag = false;
	PnxMediaTime first_video_time;
	PnxMediaTime last_video_time;
	int num_i_frames = 0;
	for (std::shared_ptr<StreamBuffer> stream_buf = group->PopQueue(); stream_buf != nullptr; stream_buf = group->PopQueue()) {
		CString gmtTime, localTime;
		ConvertEpochToGMTAndLocalCString(stream_buf->timestamp_msec, gmtTime, localTime);

		TRACE("time: %lld, GMT: %s, Local: %s\n",
			stream_buf->timestamp_msec,
			(LPCTSTR)gmtTime,
			(LPCTSTR)localTime);
	}
}

void CFileLoaderTab::OnBnClickedBtnFileloadNext()
{
	std::shared_ptr<PnxMediaDataGroup> datas = GetNextData(read_object_, 10, kArchiveTypeNone);
	if (datas == nullptr)
		return;

	while (datas->GetNumber() > 0) {
		auto data = datas->PopFront();
		data->profile = MediaProfile::kAuto;
		CString gmtTime, localTime;
		ConvertEpochToGMTAndLocalCString(data->time_info.ToMilliSeconds(), gmtTime, localTime);

		TRACE("time: %lld, GMT: %s, Local: %s\n",
			data->time_info.ToMilliSeconds(),
			(LPCTSTR)gmtTime,
			(LPCTSTR)localTime);
	}
}


void CFileLoaderTab::InitSeeker() {
	reader_worker_.SetData("/Hanwha Vision/BLAZE server/Archive/257fa333-0b4d-4e2a-8a14-3db873078497/c0908d4b-e331-4688-b5fe-328eb7804dc1");
	reader_worker_.AddDrive("D:");
}
bool CFileLoaderTab::Seeker(ChannelUUID channel, const PnxMediaTime& time, ArchiveReadType archive_read_type, std::shared_ptr<ReaderObject> read_object)
{
	if (read_object == nullptr)
		return false;
	bool is_seek = true;
	/*SPDLOG_INFO("[ArchiveL] Seek : channel : {}, time : {}, archive_read_type : {}, profile : {}", channel, time.ToMilliSeconds(), archive_read_type,
		read_object->GetProfile().ToString());*/
	if (kArchiveReadNext == archive_read_type) {
		reader_worker_.Seek(channel, time.ToMilliSeconds(), kArchiveReadPrev, read_object);
		SPDLOG_INFO("[ArchiveL] Seek Next 1");
		if (read_object->IsInGOP(time)) {
			read_object->ClearFileList();
			while (true) {
				if (read_object->GetChunkEndTime() < time.ToMilliSeconds()) {
					auto frame_data = reader_worker_.GetNextData(kArchiveTypeFrameVideo, read_object);
					if (frame_data == std::nullopt) {
						SPDLOG_ERROR("[ArchiveL] Seek Next frame null");
						is_seek = false;
						break;
					}
				}
				else
					break;
			}

			SPDLOG_INFO("[ArchiveL] Seek Next in Gop");
			return is_seek;
		}
	}
	SPDLOG_INFO("[ArchiveL] Seek - ");
	return reader_worker_.Seek(channel, time.ToMilliSeconds(), archive_read_type, read_object);
}

std::shared_ptr<ArchiveChunkBuffer> CFileLoaderTab::GetStreamGop(ArchiveChunkReadType gov_read_type, std::shared_ptr<ReaderObject> read_object) {
	auto chunk_buffer = reader_worker_.GetStreamGop(gov_read_type, read_object);
	if (chunk_buffer && read_object) {
		SPDLOG_INFO("[ArchiveManager] GetStreamGop ch_uuid: {}, ArchiveChunkReadType: {}, pos(ms): {}, chunk_begin(ms): {}, chunk_end(ms): {}",
			read_object->GetSessionid(), static_cast<int>(gov_read_type), read_object->GetCurrentPositionTime(), chunk_buffer->GetChunkBeginTime(),
			chunk_buffer->GetChunkEndTime());
	}
	return chunk_buffer;
}
std::shared_ptr<PnxMediaDataGroup> CFileLoaderTab::GetNextData(std::shared_ptr<ReaderObject> read_object,
	const unsigned char cseq, ArchiveType get_video_type) {
	std::optional<std::vector<std::shared_ptr<StreamBuffer>>> datas = reader_worker_.GetNextData(get_video_type, read_object);
	if (datas == std::nullopt)
		return nullptr;

	//SPDLOG_INFO("[ArchiveL] GetNextData : frame time : {}\n", datas->begin()->get()->timestamp_msec);
	std::shared_ptr<PnxMediaDataGroup> media_datas = std::make_shared<PnxMediaDataGroup>();

	for (auto data : datas.value()) {
		if (auto video = std::dynamic_pointer_cast<VideoData>(data)) {
			auto video_data = std::dynamic_pointer_cast<Pnx::Media::VideoSourceFrame>(video.get()->buffer);
			if (video_data == nullptr)
				continue;

			video_data->codecType = video.get()->archive_header.codecType;
			video_data->frameType = video.get()->archive_header.frameType;
			video_data->resolution.height = video.get()->archive_header.height;
			video_data->resolution.width = video.get()->archive_header.width;
			video_data->pts = video.get()->timestamp_msec;

			auto pnx_data = std::make_shared<PnxMediaData>();
			pnx_data->type = Pnx::Media::MediaType::Video;
			pnx_data->profile = read_object->GetProfile();
			pnx_data->phoenix_play_info.cseq = cseq;
			pnx_data->data = video_data;
			pnx_data->time_info.FromMilliSeconds(video_data->pts);
			media_datas->PushBack(pnx_data);
		}
		else if (auto audio = std::dynamic_pointer_cast<AudioData>(data)) {
			auto audio_data = std::dynamic_pointer_cast<Pnx::Media::AudioSourceFrame>(audio.get()->buffer);
			if (audio_data == nullptr)
				continue;

			audio_data->codecType = audio.get()->archive_header.codecType;
			audio_data->audioChannels = audio.get()->archive_header.audioChannels;
			audio_data->audioSampleRate = audio.get()->archive_header.audioSampleRate;
			audio_data->audioBitPerSample = audio.get()->archive_header.audioBitPerSample;
			audio_data->audioBitrate = audio.get()->archive_header.audioBitrate;
			audio_data->pts = audio.get()->timestamp_msec;

			auto pnx_data = std::make_shared<PnxMediaData>();
			pnx_data->type = Pnx::Media::MediaType::Audio;
			pnx_data->profile = read_object->GetProfile();
			pnx_data->phoenix_play_info.cseq = cseq;
			pnx_data->data = audio_data;
			pnx_data->time_info.FromMilliSeconds(audio_data->pts);
			media_datas->PushBack(pnx_data);
		}
		else if (auto meta = std::dynamic_pointer_cast<MetaData>(data)) {
			auto meta_data = std::dynamic_pointer_cast<Pnx::Media::MetaDataSourceFrame>(meta.get()->buffer);
			if (meta_data == nullptr)
				continue;

			meta_data->pts = meta.get()->timestamp_msec;

			auto pnx_data = std::make_shared<PnxMediaData>();
			pnx_data->type = Pnx::Media::MediaType::MetaData;
			pnx_data->profile = read_object->GetProfile();
			pnx_data->phoenix_play_info.cseq = cseq;
			pnx_data->data = meta_data;
			pnx_data->time_info.FromMilliSeconds(meta_data->pts);
			media_datas->PushBack(pnx_data);
		}
	}

	return media_datas;
}

