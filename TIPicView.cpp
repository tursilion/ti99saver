// TIPicView.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <stdio.h>
#include <math.h>
#include <process.h>
#include "C:\WORK\imgsource\4.0\islibs40_vs05\ISource.h"
#include "C:\WORK\imgsource\2.1\src\ISLib\isarray.h"
#include <windows.h>
#include <list>

#pragma pack(1)

#include "TIPicView.h"
#include "TIPicViewDlg.h"

// define this to have non-perceptual match using YCrCb instead of RGB
#define USEYCRCB

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

typedef unsigned int UINT32;
typedef unsigned char uchar;
typedef unsigned long ulong;
typedef unsigned long slong;
typedef short sshort;
typedef char schar;

// really nasty - separate 16-color palette per scanline
int g_MatchColors = 15;				// number of non-paletted colors to be matched (used for B&W and greyscale)

static uchar pal[256][4];	// RGB0
static double YUVpal[256][4];	// YCrCb0 - misleadingly called YUV for easier typing
uchar scanlinepal[192][16][4];	// RGB0
int pixels[32][8][4];
extern MYRGBQUAD palinit16[256];
int cols[4096];				// work space

// check LoadSettings for defaults
volatile int nCurrentByte;
int PIXA,PIXB,PIXC,PIXD,PIXE,PIXF;
int g_nFilter=4;
int g_nPortraitMode=1;			// 0=fit, 1=top/left, 2=middle, 3=bottom/right
int g_Perceptual=0;				// enable perceptual RGB color matching instead of YCrCb
int g_AccumulateErrors=1;		// accumulate errors instead of averaging (old method)
double g_PercepR=0.30, g_PercepG=0.52, g_PercepB=0.18;
int LineLoad = 64;

extern unsigned char buf8[256*192];
extern RGBQUAD winpal[256];

void quantize_new(BYTE* pRGB, BYTE* p8Bit);

// pRGB - input image - 256x192x24-bit image
// p8Bit - output image - 256x192x8-bit image, we will provide palette
// pal - palette to use (15-color TI palette, color 0 (transparent) not included, so 0-14 are valid
// many global flags override
void MYRGBTo8BitDithered(BYTE *pRGB, BYTE *p8Bit, MYRGBQUAD *inpal)
{
	// prepare palette (this will be overridden later if needed)
	for (int i=0; i<15; i++) {
		pal[i][0]=inpal[i].rgbRed;
		pal[i][1]=inpal[i].rgbGreen;
		pal[i][2]=inpal[i].rgbBlue;

		// Ey = 0.299R + 0.587G + 0.114B
		// Ecr = 0.713(R - Ey) = 0.500R - 0.419G - 0.081B
		// Ecb = 0.564(B - Er) = -0.169R - 0.331G + 0.500B
		// Normally expects RGB to be 0.0-1.0, outputs Y=0.0-1.0, CrCb=-0.5 to + 0.5 (so our values are scaled to 255)
		YUVpal[i][0]= 0.299*pal[i][0] + 0.587 * pal[i][1] + 0.114 * pal[i][2];
		YUVpal[i][1]= 0.500*pal[i][0] - 0.419 * pal[i][1] - 0.081 * pal[i][2];
		YUVpal[i][2]=-0.169*pal[i][0] - 0.331 * pal[i][1] + 0.500 * pal[i][2];
	}

	// this function handles it all
	quantize_new(pRGB, p8Bit);
}

// pIn is a 24-bit RGB image
// pOut is an 8-bit palettized image, with the palette in 'pal'
// pOut contains a multicolor mapped image if half-multicolor is on
// CurrentBest is the current closest value and is used to abort a search without
// having to check all 8 pixels for a small speedup when the color is close
void quantize_new(BYTE* pRGB, BYTE* p8Bit) {
	int idx;
	int row,col;
	double nDesiredPixels[8][3];		// there are 8 RGB pixels to match

	// timing report
	time_t nStartTime, nEndTime;
	time(&nStartTime);

	// create some workspace
	double *pError = (double*)malloc(sizeof(double)*258*193*3);	// error map, 3 colors, includes 1 pixel border on all sides but top (so first entry is x=-1, y=0, and each row is 258 pixels wide)
	for (idx=0; idx<258*193*3; idx++) {
		pError[idx]=0.0;
	}

	// go ahead and get to work
	for (row = 0; row < 192; row++) {
		MSG msg;
		printf("Processing row %d...\r", row);

		// see whether we are being asked to shut down?
#if 1
		// Keyboard & mouse messages are not the /only/ way for it to end, but they are most of the ways
		if (PeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_NOREMOVE|PM_NOYIELD)) {
			printf("Abort!\n");
			break;
		}
		if (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE|PM_NOYIELD)) {
			printf("Abort!\n");
			break;
		}
#else
		if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE|PM_NOYIELD)) {
			CWinApp *p = AfxGetApp();
			if (p) {
				p->PumpMessage();
			}
		}
#endif

		// divisor for the error value. Three errors are added together except for the left
		// column (only 2), and top row (only 1). The top left pixel has none, but no divisor
		// is needed since the stored value is also zero.
		double nErrDivisor = 3.0;
		if ((row == 0) || (g_AccumulateErrors)) nErrDivisor=1.0;

		for (col = 0; col < 256; col+=8) {
			// address of byte in the RGB image (3 bytes per pixel)
			BYTE *pInLine = pRGB + (row*256*3) + (col*3);
			// address of byte in the 8-bit image (1 byte per pixel)
			BYTE *pOutLine = p8Bit + (row*256) + col;
			// address of entry in the error table (3 doubles per pixel and larger array)
			double *pErrLine = pError + (row*258*3) + ((col+1)*3);

			// our first job is to get the desired pixel pattern for this block of 8 pixels
			// This takes the original image, and adds in the data from the error map. We do
			// all matching in double mode now.
			for (int c=0; c<8; c++, pInLine+=3, pErrLine+=3) {
				nDesiredPixels[c][0] = (double)(*pInLine);		// red
				nDesiredPixels[c][1] = (double)(*(pInLine+1));	// green
				nDesiredPixels[c][2] = (double)(*(pInLine+2));	// blue

				// don't dither if the desired color is pure black or white (helps reduce error spread)	
				if ((nDesiredPixels[c][0]==0)&&(nDesiredPixels[c][1]==0)&&(nDesiredPixels[c][2]==0)) continue;
				if ((nDesiredPixels[c][0]==0xff)&&(nDesiredPixels[c][1]==0xff)&&(nDesiredPixels[c][2]==0xff)) continue;

				// add in the error table for these pixels
				// Technically pixel 0 on each row except 0 should be /2, this will do /3, but that is close enough
				nDesiredPixels[c][0]+=(*pErrLine)/nErrDivisor;		// we don't want to clamp - that induces color shift
				nDesiredPixels[c][1]+=(*(pErrLine+1))/nErrDivisor;
				nDesiredPixels[c][2]+=(*(pErrLine+2))/nErrDivisor;
			}

			// tracks the best distance (starts out bigger than expected)
			double nBestDistance = 256.0*256.0*256.0*256.0;
			int nBestFg=0;
			int nBestBg=0;
			int nBestPat=0;
			double nErrorOutput[6];				// saves the error output for the next horizontal pixel (vertical only calculated on the best match)
			// zero the error output (will be set to the best match only)
			for (int i1=0; i1<6; i1++) {
				nErrorOutput[i1]=0.0;
			}

			// start searching for the best match
			for (int fg=0; fg<(g_MatchColors==2?1:g_MatchColors); fg++) {							// foreground color
				// happens 15 times
				for (int bg=fg+1; bg<g_MatchColors; bg++) {						// background color
					// happens 120 times (at most)
					for (int pat=((bg==fg)?255:0); pat<256; pat++) {	// pixel pattern (start at 0xff when colors are equal)
						// happens 30,705 times
						double t1,t2,t3;
						double tmpR, tmpG, tmpB, farR, farG, farB;
						double nCurDistance;
						int nMask=0x80;

						// begin the search
						nCurDistance = 0.0;
						tmpR=0.0;
						tmpG=0.0;
						tmpB=0.0;
						farR=0.0;
						farG=0.0;
						farB=0.0;

						for (int bit=0; bit<8; bit++) {					// pixel index into pattern
							// happens 245,640 times
							int nCol;
							// which color to use
							if (pat&nMask) {
								// use FG color
								nCol = fg;
							} else {
								nCol = bg;
							}

							// calculate actual error for this pixel
							// Note: wrong divisor for pixel 0 on each row except 0 (should be 2 but will be 3), 
							// but we'll live with it for now
							// update the total error
							if (g_Perceptual) {
								t1=(nDesiredPixels[bit][0]+(tmpR/nErrDivisor))-pal[nCol][0];
								t2=(nDesiredPixels[bit][1]+(tmpG/nErrDivisor))-pal[nCol][1];
								t3=(nDesiredPixels[bit][2]+(tmpB/nErrDivisor))-pal[nCol][2];

								nCurDistance+=(t1*t1)*g_PercepR+(t2*t2)*g_PercepG+(t3*t3)*g_PercepB;
							} else {
#ifdef USEYCRCB
								double r,g,b;
								double y,cr,cb;
								// get RGB
								r=(nDesiredPixels[bit][0]+(tmpR/nErrDivisor));
								g=(nDesiredPixels[bit][1]+(tmpG/nErrDivisor));
								b=(nDesiredPixels[bit][2]+(tmpB/nErrDivisor));
								// make YCrCb
								y  = 0.299*r + 0.587*g + 0.114*b;
								cr = 0.500*r - 0.419*g - 0.081*b;
								cb =-0.169*r - 0.331*g + 0.500*b;
								// gets diffs
								t1=y-YUVpal[nCol][0];
								t2=cr-YUVpal[nCol][1];
								t3=cb-YUVpal[nCol][2];
								nCurDistance+=(t1*t1)+(t2*t2)+(t3*t3);
#endif

								// we need the RGB diffs for the error diffusion anyway
								t1=(nDesiredPixels[bit][0]+(tmpR/nErrDivisor))-pal[nCol][0];
								t2=(nDesiredPixels[bit][1]+(tmpG/nErrDivisor))-pal[nCol][1];
								t3=(nDesiredPixels[bit][2]+(tmpB/nErrDivisor))-pal[nCol][2];

#ifndef USEYCRCB
								nCurDistance+=(t1*t1)+(t2*t2)+(t3*t3);
#endif
							}

							if (nCurDistance >= nBestDistance) {
								// no need to search this one farther
								break;
							}

							// update expected horizontal error based on new actual error
							tmpR=(t1*(double)PIXD)/16.0;
							tmpG=(t2*(double)PIXD)/16.0;
							tmpB=(t3*(double)PIXD)/16.0;

							// migrate the far pixel in
							tmpR+=farR;
							tmpG+=farG;
							tmpB+=farB;

							// calculate the next far pixel
							farR=(t1*(double)PIXE)/16.0;
							farG=(t2*(double)PIXE)/16.0;
							farB=(t3*(double)PIXE)/16.0;

							// next bit position
							nMask>>=1;
						}

						// this byte is tested, did we find a better match?
						if (nCurDistance < nBestDistance) {
							// we did! So save the data off
							nBestDistance = nCurDistance;
							nErrorOutput[0] = tmpR;
							nErrorOutput[1] = tmpG;
							nErrorOutput[2] = tmpB;
							nErrorOutput[3] = farR;
							nErrorOutput[4] = farG;
							nErrorOutput[5] = farB;
							nBestFg=fg;
							nBestBg=bg;
							nBestPat=pat;
						}
					}
				}
			}

			// at this point, we have a best match for this byte
			// first we update the error map to the right with the error output
			*(pErrLine)+=nErrorOutput[0];
			*(pErrLine+1)+=nErrorOutput[1];
			*(pErrLine+2)+=nErrorOutput[2];
			*(pErrLine+3)+=nErrorOutput[3];
			*(pErrLine+4)+=nErrorOutput[4];
			*(pErrLine+5)+=nErrorOutput[5];

			// point to the next line, same x pixel as the first one here
			pErrLine += (258*3) - (8*3);

			// now we have to output the pixels AND update the error map
			int nMask = 0x80;
			for (int bit=0; bit<8; bit++) {
				int nCol;
				double err;

				if (nBestPat&nMask) {
					nCol = nBestFg;
				} else {
					nCol = nBestBg;
				}

				nMask>>=1;

				*(pOutLine++) = (BYTE)nCol;
				bool useF = (row < 190);

				// and update the error map 
				err = nDesiredPixels[bit][0] - pal[nCol][0];		// red
				*(pErrLine+(bit*3)-3) += (err*(double)PIXA)/16.0;
				*(pErrLine+(bit*3)) += (err*(double)PIXB)/16.0;
				*(pErrLine+(bit*3)+3) += (err*(double)PIXC)/16.0;
				if (useF) {
					*(pErrLine+(bit*3)+(258*3)) += (err*(double)PIXF)/16.0;
				}

				err = nDesiredPixels[bit][1] - pal[nCol][1];		// green
				*(pErrLine+(bit*3)-2) += (err*(double)PIXA)/16.0;
				*(pErrLine+(bit*3)+1) += (err*(double)PIXB)/16.0;
				*(pErrLine+(bit*3)+4) += (err*(double)PIXC)/16.0;
				if (useF) {
					*(pErrLine+(bit*3)+(258*3)+1) += (err*(double)PIXF)/16.0;
				}

				err = nDesiredPixels[bit][2] - pal[nCol][2];		// blue
				*(pErrLine+(bit*3)-1) += (err*(double)PIXA)/16.0;
				*(pErrLine+(bit*3)+2) += (err*(double)PIXB)/16.0;
				*(pErrLine+(bit*3)+5) += (err*(double)PIXC)/16.0;
				if (useF) {
					*(pErrLine+(bit*3)+(258*3)+2) += (err*(double)PIXF)/16.0;
				}
			}
		}
	}

	// finished! clean up
	free(pError);

	time(&nEndTime);
	printf("Conversion time: %d seconds\n", nEndTime-nStartTime);
}

