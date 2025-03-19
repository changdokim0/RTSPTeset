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
#define LIST_ITEM_EPOCHTIME		4
#define LIST_ITEM_TIME			5
#define LIST_ITEM_LOCALTIME			6

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
	CMyVideoWnd m_videoWnd;
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
	afx_msg void OnBnClickedBtnFileloaderFileopen();
	CEdit m_FileSelectPath;

	void OpenFileLoad(CString filePath);
	afx_msg void OnBnClickedBtnFileloadSeek();
	CListCtrl m_listCtrl;
	afx_msg void OnBnClickedButton1();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	void CopyToClipboard();
	void PasteFromClipboard();
};
