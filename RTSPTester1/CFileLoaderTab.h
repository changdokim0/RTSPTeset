#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "afxdialogex.h"
#include "reader_object.h"
#include "reader_worker.h"
#include <atlstr.h> 
#include <ctime>   
#include "CD3dVideoWnd.h"
#include "FFmpegWrapper.h"

// CFileLoaderTab 대화 상자
#define LIST_ITEM_INDEX		0
#define LIST_ITEM_TYPE		1
#define LIST_ITEM_CODECTYPE		2
#define LIST_ITEM_FRAMETYPE		3
#define LIST_ITEM_FRMAESIZE		4
#define LIST_ITEM_PACKETSIZE	5
#define LIST_ITEM_EPOCHTIME		6
#define LIST_ITEM_TIME			7
#define LIST_ITEM_LOCALTIME		8

class CFileLoaderTab : public CDialogEx
{
	DECLARE_DYNAMIC(CFileLoaderTab)

public:
	CFileLoaderTab(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~CFileLoaderTab();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_FILELOADER_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	DECLARE_MESSAGE_MAP()

	CString strPathIni;
	CustomPictureControl m_videoWnd;
	FFmpegWrapper ffmpegWrapper;
	//ReaderWorker reader_worker_;
	std::shared_ptr<ReaderObject> read_object_;
	std::vector<std::optional<std::vector<std::shared_ptr<StreamBuffer>>>> buffers_all_;
	CEdit m_FileSeekTimeEdit;
	HTREEITEM tree_root_;
	void MakeTree();
	void DeleteAllChildItems(CTreeCtrl& treeCtrl);

	CString GetTypeString(int type);
	void ConvertEpochToGMTAndLocalCString(long long epochTime, CString& gmtStr, CString& localStr);
public:
	virtual BOOL OnInitDialog();

	CEdit m_FileSelectPath;
	int m_prevSelectedItem = -1;

	void OpenFileLoad(CString filePath);
	CListCtrl m_listCtrl;
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	void CopyToClipboard();
	void PasteFromClipboard();
	void HandleListItemSelection();

	afx_msg void OnBnClickedBtnFileloaderFileopen();
	afx_msg void OnBnClickedBtnFileloadSeek();
	afx_msg void OnBnClickedButton1();
	afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	CEdit m_edt_box;
};
