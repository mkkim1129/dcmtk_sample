
// DICOMModalityDlg.h : header file
//

#pragma once


// CDICOMModalityDlg dialog
class CDICOMModalityDlg : public CDialogEx
{
// Construction
public:
	CDICOMModalityDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DICOMMODALITY_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CString m_scp_ip;
	int m_scp_port;
	CString m_scp_aetitle;
	CString m_query;
	CString m_response;
	afx_msg void OnBnClickedButtonRequest();
	afx_msg void OnDestroy();

private:
	CString GenerateTempFileName() const;
};
