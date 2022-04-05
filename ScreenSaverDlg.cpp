//
// (C) 2004 Mike Brent aka Tursi aka HarmlessLion.com
// This software is provided AS-IS. No warranty
// express or implied is provided.
//
// This notice defines the entire license for this code.
// All rights not explicity granted here are reserved by the
// author.
//
// You may redistribute this software provided the original
// archive is UNCHANGED and a link back to my web page,
// http://harmlesslion.com, is provided as the author's site.
// It is acceptable to link directly to a subpage at harmlesslion.com
// provided that page offers a URL for that purpose
//
// Source code, if available, is provided for educational purposes
// only. You are welcome to read it, learn from it, mock
// it, and hack it up - for your own use only.
//
// Please contact me before distributing derived works or
// ports so that we may work out terms. I don't mind people
// using my code but it's been outright stolen before. In all
// cases the code must maintain credit to the original author(s).
//
// -COMMERCIAL USE- Contact me first. I didn't make
// any money off it - why should you? ;) If you just learned
// something from this, then go ahead. If you just pinched
// a routine or two, let me know, I'll probably just ask
// for credit. If you want to derive a commercial tool
// or use large portions, we need to talk. ;)
//
// If this, itself, is a derived work from someone else's code,
// then their original copyrights and licenses are left intact
// and in full force.
//
// http://harmlesslion.com - visit the web page for contact info
//
// ScreenSaverDlg.cpp : implementation file
//
//////////
//
// Copyright (C) 1991-98 Ed Halley.
//   http://www.explorati.com/people/ed/
//   ed@explorati.com
//
// This published source code represents original intellectual
// property, owned and copyrighted by Ed Halley.
//
// The owner has authorized this source code for general public
// use without royalty, under two conditions:
//    * The source code maintains this copyright notice in full.
//    * The source code is only distributed for free or
//      reasonable duplication cost, not for distribution profit.
//
// Unauthorized use, copying or distribution is a violation of
// U.S. and international laws and is strictly prohibited.
//
//////////
//

#include "StdAfx.h"

#include "ScreenSaverWnd.h"
#include "ScreenSaverDlg.h"
#include "TIPicViewDlg.h"
#include "MfcSaver.h"
#include "MfcSaverDlg.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CMfcSaver theSaver;

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CScreenSaverDlg, CDialog)

// MFC's normal processing would have CDialog and CWnd handle any messages
// not handled by the CScreenSaverDlg.  Some are not appropriate for a
// screen saver's configuration dialog, so we skip CDialog and CWnd in the
// line below, and have all unhandled messages go to the CCmdTarget.
// The WindowProc() below will forward the reasonable messages to CDialog.
//
BEGIN_MESSAGE_MAP(CScreenSaverDlg, CCmdTarget)
	//{{AFX_MSG_MAP(CScreenSaverDlg)
	ON_WM_NCDESTROY()
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
	ON_COMMAND(IDOK, OnOK)
	ON_COMMAND(IDCANCEL, OnCancel)
	ON_MESSAGE(WM_INITDIALOG, HandleInitDialog)
END_MESSAGE_MAP()

// There should be only one CScreenSaverDlg instantiated at a time.
// The helper function AfxGetScreenSaverDlg() returns this pointer.
//
/* static */ CScreenSaverDlg* CScreenSaverDlg::sm_pTheConfigureDialog = NULL;

CScreenSaverDlg::CScreenSaverDlg()
{
	ASSERT(!sm_pTheConfigureDialog);
	sm_pTheConfigureDialog = this;
}

CScreenSaverDlg::~CScreenSaverDlg()
{
	sm_pTheConfigureDialog = NULL;
}

/////////////////////////////////////////////////////////////////////////////

void CScreenSaverDlg::OnNcDestroy()
{
	CDialog::OnNcDestroy();
}

BOOL CScreenSaverDlg::OnInitDialog()
{
	return CDialog::OnInitDialog();
}

int CScreenSaverDlg::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	return 0;
}

/////////////////////////////////////////////////////////////////////////////

//
// The default message routing is not built to deal with screen saver
// dialogs.  This avoids the default dialog proc from handling anything we
// don't handle.
//

LRESULT CScreenSaverDlg::WindowProc(UINT uMsg,
                                    WPARAM wParam,
                                    LPARAM lParam)
{
	return CDialog::WindowProc(uMsg, wParam, lParam);
}

LRESULT CScreenSaverDlg::DefWindowProc(UINT uMsg,
                                       WPARAM wParam,
                                       LPARAM lParam)
{
	return 0L;
}

/////////////////////////////////////////////////////////////////////////////

// ::ScreenSaverConfigureDialog():
// This API must be exported to be recognized as a configurable screen
// saver.  This serves as the dialog message proc called by Windows to
// display the configuration dialog for the screen saver.  Not all messages
// that go to the dialog actually get sent to our proc.
//
BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT uMsg,
                                       WPARAM wParam, LPARAM lParam)
{

	// There should be one global instance of a CScreenSaverDlg derivative.
	ASSERT(AfxGetScreenSaverDialog());
	if (!AfxGetScreenSaverDialog())
		return 0L;

	if (!AfxGetScreenSaverDialog()->m_hWnd)
		AfxGetScreenSaverDialog()->Attach(hDlg);

	// MFC does a lot of helpful but nonstandard things with WM_INITDIALOG.
	//
	// Since a screen saver's configuration dialog proc is dictated to us,
	// we need to manually perform the ->OnInitDialog() call ourselves.
	//
	// This is equivalent to MFC4.2's ::AfxDlgProc() [which is not exported
	// nor used by the sanctioned exported public ::AfxCallWndProc()
	// function].
	//
	LRESULT lResult;
	if (uMsg == WM_INITDIALOG)
	{
		CDialog* pDlg =
			DYNAMIC_DOWNCAST(CDialog,
				CWnd::FromHandlePermanent(hDlg));
		if (pDlg == NULL) {
			return 1;	// what more can we do?
		}
		
		lResult = pDlg->OnInitDialog();
		((CMfcSaverDlg*)pDlg)->LoadData();
	}
	else
	{
		lResult =
			AfxCallWndProc(
				AfxGetScreenSaverDialog(),
				AfxGetScreenSaverDialog()->m_hWnd,
		        uMsg, wParam, lParam);
	}

	if (uMsg == WM_NCDESTROY)
	{
		ASSERT(!AfxGetScreenSaverDialog() ||
		       !AfxGetScreenSaverDialog()->m_hWnd);
	}

	if ((uMsg == WM_COMMAND) && (wParam == IDOK)) {
		CDialog* pDlg =
			DYNAMIC_DOWNCAST(CDialog,
				CWnd::FromHandlePermanent(hDlg));
		if (pDlg == NULL) {
			return 1;	// what more can we do?
		}

		// copy the settings from the dialog down to the variables, so we can save them
		((CMfcSaverDlg*)pDlg)->SaveData();
		theSaver.SaveSettings();

	}

	return lResult;
}

// ::RegisterDialogClasses():
// This API must be exported to be recognized as a configurable screen
// saver.  It is called upon startup to register any WNDCLASS structures
// that would later be necessary to display a configuration dialog.
//
BOOL WINAPI RegisterDialogClasses(HANDLE hInstance)
{
	// We rely on the default registered window class used by MFC
	// dialogs.  Therefore, we have nothing to do here.
	//
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
