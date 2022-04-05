// TIPicViewDlg.cpp : implementation file
//

#include "stdafx.h"
#include "TIPicView.h"
#include "TIPicViewDlg.h"
#include "C:\WORK\imgsource\4.0\islibs40_vs05\ISource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern bool StretchHist;
extern int PIXA,PIXB,PIXC,PIXD,PIXE,PIXF;
extern int g_nFilter;
extern int g_nPortraitMode;
extern int heightoffset;
extern int g_Perceptual;
extern int g_AccumulateErrors;
extern int g_MatchColors;
extern unsigned char scanlinepal[192][16][4];
extern double g_PercepR, g_PercepG, g_PercepB;
extern unsigned char buf8[256*192];
extern bool fFirstLoad;
extern char imagepath[MAX_PATH];

/////////////////////////////////////////////////////////////////////////////
// CTIPicViewDlg dialog

CTIPicViewDlg::CTIPicViewDlg()
{
	StretchHist=false;
	PIXA=2;
	PIXB=2;
	PIXC=2;
	PIXD=2;
	PIXE=1;
	PIXF=1;
}

/////////////////////////////////////////////////////////////////////////////
// CTIPicViewDlg message handlers

void maincode(int mode, char *pFile);

// This function doesn't get called normally, but that's okay, we can call it the long way ;)
void CTIPicViewDlg::OnDoubleclickedRnd() 
{
	LaunchMain(0,imagepath);
}

void CTIPicViewDlg::LaunchMain(int mode, char *pFile) {
	maincode(mode, pFile);
}

