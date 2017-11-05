
// DICOMModalityDlg.cpp : implementation file
//

#include "stdafx.h"
#include "DICOMModality.h"
#include "DICOMModalityDlg.h"
#include "afxdialogex.h"
#include "Dump2Dcm.h"
#include <sstream>

#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */
#include "dcmtk/dcmnet/dfindscu.h"
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmdata/cmdlnarg.h"
#include "dcmtk/ofstd/ofconapp.h"
#include "dcmtk/dcmdata/dcdict.h"
#include "dcmtk/dcmdata/dcostrmz.h"   /* for dcmZlibCompressionLevel */

#include <winsock.h>
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#pragma comment(lib, "dcmnet.lib")
#pragma comment(lib, "dcmdata.lib")
#pragma comment(lib, "oflog.lib")
#pragma comment(lib, "ofstd.lib")
#pragma comment(lib, "dcmtls.lib")


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

const char* kDefaultQueryString = 
"# Worklist query\r\n"
"#\r\n"
"(0010, 0010) PN[]                                       # PatientName\r\n"
"(0040, 0100) SQ                                          # ScheduledProcedureStepSequence\r\n"
"(fffe, e000) na                                          # Item\r\n"
"(fffe, e00d) na(ItemDelimitationItem for re - encoding)   # ItemDelimitationItem\r\n"
"(fffe, e0dd) na(SequenceDelimitationItem for re - encod.) # SequenceDelimitationItem";


class FindCallback : public DcmFindSCUCallback {
public:
	void callback(T_DIMSE_C_FindRQ *request, int responseCount, T_DIMSE_C_FindRSP *rsp, DcmDataset *responseIdentifiers) {
		responseIdentifiers->print(m_stream);
	}

	CString GetString() {
		CString result = m_stream.str().c_str();
		result.Replace("\n", "\r\n");
		return result;
	}
private:
	std::stringstream m_stream;
};

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CDICOMModalityDlg dialog



CDICOMModalityDlg::CDICOMModalityDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_DICOMMODALITY_DIALOG, pParent)
	, m_scp_ip(_T("127.0.0.1"))
	, m_scp_port(6001)
	, m_scp_aetitle(_T("OFFIS"))
	, m_query(kDefaultQueryString)
	, m_response(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CDICOMModalityDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_SCP_IP, m_scp_ip);
	DDX_Text(pDX, IDC_EDIT_SCP_PORT, m_scp_port);
	DDX_Text(pDX, IDC_EDIT_SCP_AETITLE, m_scp_aetitle);
	DDX_Text(pDX, IDC_EDIT_QUERY, m_query);
	DDX_Text(pDX, IDC_EDIT_RESPONSE, m_response);
}

BEGIN_MESSAGE_MAP(CDICOMModalityDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_REQUEST, &CDICOMModalityDlg::OnBnClickedButtonRequest)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CDICOMModalityDlg message handlers

BOOL CDICOMModalityDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	OFStandard::initializeNetwork();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CDICOMModalityDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CDICOMModalityDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CDICOMModalityDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CDICOMModalityDlg::OnBnClickedButtonRequest()
{
	UpdateData(TRUE);

	const CString filename = GenerateTempFileName();
	Dump2Dcm dump2dcm(m_query);
	dump2dcm.Save(filename);

	ASSERT(dcmDataDict.isDictionaryLoaded());

	const int opt_acse_timeout = 30;

	dcmEnableAutomaticInputDataCorrection.set(false);

	OFCondition cond;
	DcmFindSCU findscu;
	cond = findscu.initializeNetwork(opt_acse_timeout);
	ASSERT(cond.good());

	OFList<OFString> overrideKeys;
	OFList<OFString> fileNameList;
	fileNameList.push_back((LPCSTR)filename);

	FindCallback callback;
	cond = findscu.performQuery(
		m_scp_ip.GetString(),
		m_scp_port,
		"FINDSCU",
		m_scp_aetitle,
		UID_FINDModalityWorklistInformationModel,
		EXS_Unknown,
		DIMSE_BLOCKING,
		0,
		ASC_DEFAULTMAXPDU,
		false,
		false,
		1,
		false,
		-1,
		&overrideKeys,
		&callback,
		&fileNameList,
		".");
	ASSERT(cond.good());

	m_response = callback.GetString();
	
	cond = findscu.dropNetwork();
	ASSERT(cond.good());

	UpdateData(FALSE);
}


void CDICOMModalityDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	OFStandard::shutdownNetwork();
}

CString CDICOMModalityDlg::GenerateTempFileName() const
{
	char szTempFileName[MAX_PATH];
	char lpTempPathBuffer[MAX_PATH];
	DWORD dwRetVal = 0;

	dwRetVal = GetTempPath(MAX_PATH, lpTempPathBuffer);
	ASSERT(dwRetVal < MAX_PATH);
	ASSERT(dwRetVal != 0);

	dwRetVal = GetTempFileName(lpTempPathBuffer, "DCM_", 0, szTempFileName);
	return szTempFileName;
}