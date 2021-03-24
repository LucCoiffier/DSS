#ifndef _BITMAPEXT_H__
#define _BITMAPEXT_H__

#include <QCoreApplication> 
#include <QString>

#include "Multitask.h"
#include <zexcept.h>
#include "resource.h"
#include "BitmapBase.h"

#ifndef DSSFILEDECODING
#define DSSFILEDECODING 1
#endif

#define ALTERNATEHOMOGENEIZATION

/* ------------------------------------------------------------------- */

#if DSSFILEDECODING==1
bool	IsFITSRawBayer();		// From FITSUtil.h
bool	IsFITSSuperPixels();	// From FITSUtil.h
#else
inline bool	IsFITSRawBayer()	{ return false; };	// From FITSUtil.h
inline bool	IsFITSSuperPixels() { return false; };	// From FITSUtil.h
#endif // !DSSFILEDECODING


/* ------------------------------------------------------------------- */

class CDSSProgress;

/* ------------------------------------------------------------------- */

inline double ToRGB1(float rm1, float rm2, float rh)
{
  if (rh > 360.0f)
	  rh -= 360.0f;
  else if (rh <   0.0f)
	  rh += 360.0f;

  if      (rh <  60.0f)
	  rm1 = rm1 + (rm2 - rm1) * rh / 60.0f;
  else if (rh < 180.0f)
	  rm1 = rm2;
  else if (rh < 240.0f)
	  rm1 = rm1 + (rm2 - rm1) * (240.0f - rh) / 60.0f;

  return (rm1 * 255.0);
}

inline void ToRGB(double H, double S, double L, double & Red, double & Green, double & Blue)
{
    if (S == 0.0)
    {
      Red = Green = Blue = L * 255.0;
    }
    else
    {
      double rm1, rm2;

      if (L <= 0.5f)
		  rm2 = (double)(L + L * S);
      else
		  rm2 = (double)(L + S - L * S);
      rm1 = (double)(2.0f * L - rm2);

      Red   = ToRGB1(rm1, rm2, (double)(H + 120.0f));
      Green = ToRGB1(rm1, rm2, (double)H);
      Blue  = ToRGB1(rm1, rm2, (double)(H - 120.0f));
    }
};

/* Return the HSL luminance value. */
inline double GetLuminance(const COLORREF crColor)
{
	constexpr double scalingFactor = 1.0 / 256.0;

	const unsigned red = GetRValue(crColor);
	const unsigned green = GetGValue(crColor);
	const unsigned blue = GetBValue(crColor);

	const unsigned minval = std::min(red, std::min(green, blue));
	const unsigned maxval = std::max(red, std::max(green, blue));
	const unsigned msum = maxval + minval;

	return static_cast<double>(msum) * (0.5 * scalingFactor);
};

/* Return the HSL luminance value. */
inline double GetLuminance(const COLORREF16 & crColor)
{
	constexpr double scalingFactor = 1.0 / 256.0;

	const unsigned minval = std::min(crColor.red, std::min(crColor.green, crColor.blue));
	const unsigned maxval = std::max(crColor.red, std::max(crColor.green, crColor.blue));
	const unsigned msum = maxval + minval;

	return static_cast<double>(msum) * (0.5 * scalingFactor * scalingFactor); // (((double)msum / 256.0) / 510.0);
};

/* ------------------------------------------------------------------- */

inline double ClampPixel(double fValue)
{
	if (fValue<0)
		return 0;
	else if (fValue>255.0*256.0)
		return 255.0*256.0;
	else
		return fValue;
};

/* ------------------------------------------------------------------- */

class CMemoryBitmap;

/* ------------------------------------------------------------------- */

inline void	FormatFromMethod(CString & strText, MULTIBITMAPPROCESSMETHOD Method, double fKappa, LONG lNrIterations)
{
	strText.Empty();
	switch (Method)
	{
	case MBP_FASTAVERAGE :
	case MBP_AVERAGE :
		strText.Format(IDS_RECAP_AVERAGE);
		break;
	case MBP_MEDIAN :
		strText.Format(IDS_RECAP_MEDIAN);
		break;
	case MBP_MAXIMUM :
		strText.Format(IDS_RECAP_MAXIMUM);
		break;
	case MBP_SIGMACLIP :
		strText.Format(IDS_RECAP_KAPPASIGMA, fKappa, lNrIterations);
		break;
	case MBP_AUTOADAPTIVE :
		strText.Format(IDS_RECAP_AUTOADAPTIVE, lNrIterations);
		break;
	case MBP_ENTROPYAVERAGE :
		strText.Format(IDS_RECAP_ENTROPYAVERAGE);
		break;
	case MBP_MEDIANSIGMACLIP :
		strText.Format(IDS_RECAP_MEDIANSIGMACLIP, fKappa, lNrIterations);
	};
};

inline QString formatMethod(MULTIBITMAPPROCESSMETHOD Method, double fKappa, LONG lNrIterations)
{
	QString strText;

	switch (Method)
	{
	case MBP_FASTAVERAGE:
	case MBP_AVERAGE:
		strText = QCoreApplication::translate("StackRecap", "Average", "IDS_RECAP_AVERAGE");
		break;
	case MBP_MEDIAN:
		strText = QCoreApplication::translate("StackRecap", "Median", "IDS_RECAP_MEDIAN");
		break;
	case MBP_MAXIMUM:
		strText = QCoreApplication::translate("StackRecap", "Maximum", "IDS_RECAP_MAXIMUM");
		break;
	case MBP_SIGMACLIP:
		strText = QCoreApplication::translate("StackRecap", "Kappa-Sigma (Kappa = %1, Iterations = %2)",
				"IDS_RECAP_KAPPASIGMA")
			.arg(fKappa, 0, 'f', 2)
			.arg(lNrIterations);
		break;
	case MBP_AUTOADAPTIVE:
		strText = QCoreApplication::translate("StackRecap",
				"Auto Adaptive Weighted Average (Iterations = %1)",
				"IDS_RECAP_AUTOADAPTIVE")
			.arg(lNrIterations);
		break;
	case MBP_ENTROPYAVERAGE:
		strText = QCoreApplication::translate("StackRecap",
			"Entropy Weighted Average",
			"IDS_RECAP_ENTROPYAVERAGE");
		break;
	case MBP_MEDIANSIGMACLIP:
		strText = QCoreApplication::translate("StackRecap", 
				"Median Kappa-Sigma (Kappa = %1, Iterations = %2)",
				"IDS_RECAP_MEDIANSIGMACLIP")
			.arg(fKappa, 0, 'f', 2)
			.arg(lNrIterations);
	};
	return strText;
};

/* ------------------------------------------------------------------- */

bool Subtract(CMemoryBitmap * pTarget, CMemoryBitmap * pSource, CDSSProgress * pProgress = nullptr, double fRedFactor = 1.0, double fGreenFactor = 1.0, double fBlueFactor = 1.0);
bool Add(CMemoryBitmap * pTarget, CMemoryBitmap * pSource, CDSSProgress * pProgress = nullptr);
bool ShiftAndSubtract(CMemoryBitmap * pTarget, CMemoryBitmap * pSource, CDSSProgress * pProgress = nullptr, double fXShift = 0, double fYShift = 0);
bool Multiply(CMemoryBitmap * pTarget, double fRedFactor, double fGreenFactor, double fBlueFactor, CDSSProgress * pProgress = nullptr);

/* ------------------------------------------------------------------- */

inline void	CYMGToRGB2(double fCyan, double fYellow, double fMagenta, double fGreen2, double & fRed, double & fGreen, double & fBlue)
{
	double			fR, fG, fB;

	fR   = (fMagenta+fYellow-fCyan)/2.0;
	fG = (fYellow+fCyan-fMagenta)/2.0;
	fB  = (fMagenta+fCyan-fYellow)/2.0;

	fRed   = 2.088034662 * fR + -3.663103328 * fG + 3.069027325 * fB;
	fGreen = -0.28607719 * fR +	1.706598409	* fG + 0.24881043 * fB;
	fBlue  = -0.180853396 * fR + -7.714219397 * fG + 9.438903145 * fB;

	fRed = max(0.0, min (255.0, fRed));
	fGreen = max(0.0, min (255.0, fGreen));
	fBlue = max(0.0, min (255.0, fBlue));
};

inline void	CYMGToRGB3(double fCyan, double fYellow, double fMagenta, double fGreen2, double & fRed, double & fGreen, double & fBlue)
{
	fRed = fGreen = fBlue = 0;
	// Basic formulae
	// M = R + B
	// Y = R + G
	// C = B + G

	// RGB from CYM
	// R = (M+Y-C)/2
	// G = (Y+C-M)/2
	// B = (M+C-Y)/2
	fRed   = max(0.0, fMagenta+fYellow-fCyan)/2.0;
	fGreen = max(0.0, fYellow+fCyan-fMagenta)/2.0;
	fBlue  = max(0.0 ,fMagenta+fCyan-fYellow)/2.0;

/*	if (fGreen2)
	{
		fRed  *= fGreen2/fGreen;
		fBlue *= fGreen2/fGreen;
		fGreen = fGreen2;
	};
	fRed = min(fRed, 255);
	fBlue = min(fBlue, 255);
	fGreen = min(fGreen, 255);*/

	// RGB from CYG
	// G = G
	// R = Y - G
	// B = C - G
	fGreen += fGreen2;
	fRed   += max(0.0, fYellow-fGreen2);
	fBlue  += max(0.0, fCyan-fGreen2);

	// RGB from CMG
	// G = G
	// B = C - G
	// R = M - B = M - C + G
//	fGreen += fGreen2;
	fBlue  += max(0.0, fCyan-fGreen2);
	fRed   += max(0.0, fMagenta - fCyan + fGreen2);

	// RGB from YMG
	// G = G
	// R = Y - G
	// B = M - R = M - Y + G
//	fGreen += fGreen2;
	fRed   += max(0.0, fYellow - fGreen2);
	fBlue  += max(0.0, fMagenta - fYellow + fGreen2);

	// Average the results
	fRed /= 4.0;
	fBlue /= 4.0;
	fGreen /= 2.0;

	fRed   = max(0.0, fRed);
	fBlue  = max(0.0, fBlue);
	fGreen = max(0.0, fGreen);
};

/* ------------------------------------------------------------------- */

#if DSSFILEDECODING==1
class CWindowsBitmap : public CRefCount
{
private :
	HBITMAP				m_hBitmap;

public :
	CWindowsBitmap()
	{
		m_hBitmap = nullptr;
	};

	~CWindowsBitmap()
	{
		if (m_hBitmap)
			::DeleteObject(m_hBitmap);
	};

	HBITMAP		GetHBitmap()
	{
		return m_hBitmap;
	};

	void		SetHBitmap(HBITMAP hBitmap)
	{
		if (m_hBitmap)
			::DeleteObject(m_hBitmap);
		m_hBitmap = hBitmap;
	};
};

/* ------------------------------------------------------------------- */

class C32BitsBitmap : public CRefCount
{
private :
	HBITMAP				m_hBitmap;
	VOID *				m_lpBits;
	LONG				m_lWidth,
						m_lHeight;
	LPBYTE*				m_pLine;
	DWORD				m_dwByteWidth;

private :
	void	InitInternals()
	{
		if (m_pLine)
			free(m_pLine);

		m_pLine = (LPBYTE *)malloc(m_lHeight * sizeof(LPBYTE));
		if (nullptr == m_pLine)
		{
			ZOutOfMemory e("Could not allocate storage for scanline pointers");
			ZTHROW(e);
		}

		m_dwByteWidth   = (((m_lWidth * 32 + 31) & ~31) >> 3);
		LONG			y = m_lHeight - 1;

		for( LONG i = 0; y >= 0; y--, i++ )
		{
			m_pLine[i] = (LPBYTE)m_lpBits + y * m_dwByteWidth;
		}
	};

public :
	C32BitsBitmap()
	{
		m_hBitmap	= nullptr;
		m_lpBits	= nullptr;
		m_lWidth	= 0;
		m_lHeight	= 0;
		m_pLine		= nullptr;
        m_dwByteWidth = 0;
	};

	virtual ~C32BitsBitmap()
	{
		Free();
	};

	VOID *	GetBits()
	{
		return m_lpBits;
	};

	virtual LONG	Width()
	{
		return m_lWidth;
	};

	virtual LONG	Height()
	{
		return m_lHeight;
	};

	LONG	ByteWidth()
	{
		return m_dwByteWidth;
	};

	virtual void	Init(LONG lWidth, LONG lHeight)
	{
		Create(lWidth, lHeight);
	};

	HBITMAP	Create(LONG lWidth, LONG lHeight)
	{
		Free();

        HBITMAP			hBitmap;
        BITMAPINFO		bmpInfo;
        VOID *			pBits;

        memset(&bmpInfo, 0, sizeof(bmpInfo));
        bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
        bmpInfo.bmiHeader.biWidth = lWidth;
        bmpInfo.bmiHeader.biHeight= lHeight;
        bmpInfo.bmiHeader.biPlanes= 1;;
        bmpInfo.bmiHeader.biBitCount= 32;
        bmpInfo.bmiHeader.biCompression= BI_RGB;
        bmpInfo.bmiHeader.biSizeImage= 0;
        bmpInfo.bmiHeader.biXPelsPerMeter = (LONG)(96*100.0/2.54);
        bmpInfo.bmiHeader.biYPelsPerMeter = (LONG)(96*100.0/2.54);
        bmpInfo.bmiHeader.biClrUsed = 0;
        bmpInfo.bmiHeader.biClrImportant = 0;

		HDC				hDC;
		hDC = GetDC(nullptr);
        hBitmap = CreateDIBSection(hDC, &bmpInfo, 0, &pBits, nullptr, 0);
		ReleaseDC(nullptr, hDC);

		if (hBitmap)
		{
			m_hBitmap	= hBitmap;
			m_lpBits	= pBits;
			m_lWidth	= lWidth;
			m_lHeight	= lHeight;
			InitInternals();
		};

        return hBitmap;
	};

	bool	InitFrom(CMemoryBitmap * pBitmap);
	bool	CopyToClipboard();

	bool	IsEmpty()
	{
		return (m_hBitmap == nullptr);
	};

	HBITMAP GetHBITMAP()
	{
		return m_hBitmap;
	};

	void	Free()
	{
		if (m_hBitmap)
		{
			DeleteObject(m_hBitmap);
		};
		m_hBitmap	= nullptr;
		m_lpBits	= nullptr;
		if (m_pLine)
			free(m_pLine);
		m_pLine = nullptr;
	};

	HBITMAP	Detach()
	{
		HBITMAP		hResult = m_hBitmap;

		m_hBitmap = nullptr;
		Free();

		return hResult;
	};

	COLORREF	GetPixel(LONG x, LONG y)
	{
		COLORREF			crColor = RGB(0, 0, 0);

		if( (x >= 0) && (x < m_lWidth) && (y >=0) && (y < m_lHeight))
		{
			LPBYTE		pPixel = m_pLine[y] + ((x * 32) >> 3);
			DWORD		dwPixel;

			dwPixel = *(LPDWORD)pPixel;
			RGBQUAD		rgbq = *(LPRGBQUAD)&dwPixel;

			crColor = RGB(rgbq.rgbRed,rgbq.rgbGreen,rgbq.rgbBlue);
		};

		return crColor;
	};

	LPBYTE		GetPixelBase(LONG x, LONG y)
	{
		return m_pLine[y] + x * 4;
	};

	void		SetPixel(LONG x, LONG y, COLORREF crColor)
	{
		if( (x >= 0) && (x < m_lWidth) && (y >=0) && (y < m_lHeight))
		{
			LPBYTE		pPixel = m_pLine[y] + ((x * 32) >> 3);
			RGBQUAD		rgbq;

			rgbq.rgbRed			= GetRValue(crColor);
			rgbq.rgbGreen		= GetGValue(crColor);
			rgbq.rgbBlue		= GetBValue(crColor);
			rgbq.rgbReserved	= 0;

			*(LPDWORD)pPixel = *(LPDWORD)(&rgbq);
		};
	};
};

class	CGammaTransformation
{
public :
	std::vector<BYTE>		m_vTransformation;

public :
	CGammaTransformation() {};
	~CGammaTransformation() {};

	void		InitTransformation(double fBlackPoint, double fGrayPoint, double fWhitePoint);
	void		InitTransformation(double fGamma);

	bool		IsInitialized()
	{
		return (m_vTransformation.size() == 65537);
	};
};

bool	ApplyGammaTransformation(C32BitsBitmap * pOutBitmap, CMemoryBitmap * pInBitmap, CGammaTransformation & gammatrans);
#endif // DSSFILEDECODING

/* ------------------------------------------------------------------- */

#include "MedianFilterEngine.h"

/* ------------------------------------------------------------------- */

class CBitmapInfo
{
public :
	CString				m_strFileName;
	CString				m_strFileType;
	CString				m_strModel;
	LONG				m_lISOSpeed;
	LONG				m_lGain;
	double				m_fExposure;
	double				m_fAperture;
	LONG				m_lWidth;
	LONG				m_lHeight;
	LONG				m_lBitPerChannel;
	LONG				m_lNrChannels;
	bool				m_bCanLoad;
	bool				m_bFloat;
	CFATYPE				m_CFAType;
	bool				m_bMaster;
	bool				m_bFITS16bit;
	CString				m_strDateTime;
	SYSTEMTIME			m_DateTime;
	SYSTEMTIME			m_InfoTime;
	CBitmapExtraInfo	m_ExtraInfo;
	LONG				m_xBayerOffset;
	LONG				m_yBayerOffset;
	CString				m_filterName;

private :
	void	CopyFrom(const CBitmapInfo & bi)
	{
		m_strFileName	=bi.m_strFileName	;
		m_strFileType	=bi.m_strFileType	;
		m_strModel		=bi.m_strModel		;
		m_lISOSpeed		=bi.m_lISOSpeed		;
		m_lGain			=bi.m_lGain		;
		m_fExposure		=bi.m_fExposure		;
		m_fAperture     =bi.m_fAperture;
		m_lWidth		=bi.m_lWidth		;
		m_lHeight		=bi.m_lHeight		;
		m_lBitPerChannel=bi.m_lBitPerChannel;
		m_lNrChannels	=bi.m_lNrChannels	;
		m_bCanLoad		=bi.m_bCanLoad		;
		m_bFloat		=bi.m_bFloat		;
		m_CFAType		=bi.m_CFAType		;
		m_bMaster		=bi.m_bMaster		;
		m_bFITS16bit	=bi.m_bFITS16bit	;
		m_strDateTime	=bi.m_strDateTime	;
		m_DateTime		=bi.m_DateTime		;
		m_InfoTime		=bi.m_InfoTime		;
		m_ExtraInfo		=bi.m_ExtraInfo		;
		m_xBayerOffset  =bi.m_xBayerOffset;
		m_yBayerOffset	=bi.m_yBayerOffset;
		m_filterName	=bi.m_filterName;
	};

    void Init()
    {
        m_lWidth = 0;
        m_lHeight = 0;
        m_lBitPerChannel = 0;
        m_lNrChannels = 0;
        m_bCanLoad = false;
        m_CFAType = CFATYPE_NONE;
        m_bMaster = false;
        m_bFloat = false;
        m_lISOSpeed = 0;
        m_lGain = -1;
        m_fExposure = 0.0;
        m_fAperture = 0.0;
        m_bFITS16bit = false;
		m_DateTime = { 0 };
		m_InfoTime = { 0 };
		m_xBayerOffset = 0;
		m_yBayerOffset = 0;
    }

public :
	CBitmapInfo()
	{
        Init();
	};

	CBitmapInfo(const CBitmapInfo & bi)
	{
		CopyFrom(bi);
	};

	CBitmapInfo(LPCTSTR szFileName)
	{
        Init();

		m_strFileName = szFileName;
	};

	CBitmapInfo & operator = (const CBitmapInfo & bi)
	{
		CopyFrom(bi);
		return (*this);
	};

	bool operator < (const CBitmapInfo & bi) const
	{
		return (m_strFileName.CompareNoCase(bi.m_strFileName) < 0);
	};

	virtual ~CBitmapInfo()
	{
	};

	bool	CanLoad()
	{
		return m_bCanLoad;
	};

	bool	IsCFA()
	{
		return (m_CFAType != CFATYPE_NONE);
	};

	bool	IsMaster()
	{
		return m_bMaster;
	};

	void	GetDescription(CString & strDescription)
	{
		strDescription = m_strFileType;
		if (m_strModel.GetLength())
			strDescription.Format(_T("%s (%s)"), (LPCTSTR)m_strFileType, (LPCTSTR)m_strModel);
	};

	bool	IsInitialized()
	{
		return m_lWidth && m_lHeight;
	};
};

inline bool CompareBitmapInfoDateTime (const CBitmapInfo & bi1, const CBitmapInfo & bi2)
{
	if (bi1.m_DateTime.wYear < bi2.m_DateTime.wYear)
		return true;
	else if (bi1.m_DateTime.wYear > bi2.m_DateTime.wYear)
		return false;
	else if (bi1.m_DateTime.wMonth < bi2.m_DateTime.wMonth)
		return true;
	else if (bi1.m_DateTime.wMonth > bi2.m_DateTime.wMonth)
		return false;
	else if (bi1.m_DateTime.wDay < bi2.m_DateTime.wDay)
		return true;
	else if (bi1.m_DateTime.wDay > bi2.m_DateTime.wDay)
		return false;
	else if (bi1.m_DateTime.wHour < bi2.m_DateTime.wHour)
		return true;
	else if (bi1.m_DateTime.wHour > bi2.m_DateTime.wHour)
		return false;
	else if (bi1.m_DateTime.wMinute < bi2.m_DateTime.wMinute)
		return true;
	else if (bi1.m_DateTime.wMinute > bi2.m_DateTime.wMinute)
		return false;
	else if (bi1.m_DateTime.wSecond < bi2.m_DateTime.wSecond)
		return true;
	else if (bi1.m_DateTime.wSecond > bi2.m_DateTime.wSecond)
		return false;
	else
		return (bi1.m_DateTime.wMilliseconds < bi2.m_DateTime.wMilliseconds);
};

/* ------------------------------------------------------------------- */

#if DSSFILEDECODING==1

class	CAllDepthBitmap
{
public :
	bool						m_bDontUseAHD;

public :
	CSmartPtr<CMemoryBitmap>	m_pBitmap;
	CSmartPtr<C32BitsBitmap>	m_pWndBitmap;

private :
	void	CopyFrom(const CAllDepthBitmap & adb)
	{
		m_bDontUseAHD	= adb.m_bDontUseAHD;
		m_pBitmap		= adb.m_pBitmap;
		m_pWndBitmap	= adb.m_pWndBitmap;
	};

public :
    CAllDepthBitmap() : m_bDontUseAHD(false) {};
	~CAllDepthBitmap() {};
	CAllDepthBitmap(const CAllDepthBitmap & adb)
	{
		CopyFrom(adb);
	};

	CAllDepthBitmap & operator = (const CAllDepthBitmap & adb)
	{
		CopyFrom(adb);
		return (*this);
	};

	void	Clear()
	{
		m_pBitmap.Release();
		m_pWndBitmap.Release();
	};

	void	SetDontUseAHD(bool bSet)
	{
		m_bDontUseAHD = bSet;
	};
};

void	CopyBitmapToClipboard(HBITMAP hBitmap);

bool	RetrieveEXIFInfo(LPCTSTR szFileName, CBitmapInfo & BitmapInfo);
//HBITMAP LoadPicture(LPCTSTR szFileName, CMemoryBitmap ** ppBitmap = nullptr);
bool	LoadPicture(LPCTSTR szFileName, CAllDepthBitmap & AllDepthBitmap, CDSSProgress * pProgress = nullptr);
bool	DebayerPicture(CMemoryBitmap * pInBitmap, CMemoryBitmap ** ppOutBitmap, CDSSProgress * pProgress);

#endif // DSSFILEDECODING
bool	LoadPicture(LPCTSTR szFileName, CMemoryBitmap ** ppBitmap, CDSSProgress * pProgress);

bool	GetPictureInfo(LPCTSTR szFileName, CBitmapInfo & BitmapInfo);

bool	GetFilteredImage(CMemoryBitmap * pInBitmap, CMemoryBitmap ** ppOutBitmap, LONG lFilterSize, CDSSProgress * pProgress = nullptr);

/* ------------------------------------------------------------------- */

inline bool	IsCFA(CMemoryBitmap * pBitmap)
{
	if (pBitmap)
		return pBitmap->IsCFA();
	else
		return false;
};

/* ------------------------------------------------------------------- */

CFATYPE	GetCFAType(CMemoryBitmap * pBitmap);

/* ------------------------------------------------------------------- */

inline bool IsMonochrome(CMemoryBitmap * pBitmap)
{
	if (pBitmap)
		return pBitmap->IsMonochrome();
	else
		return false;
};

/* ------------------------------------------------------------------- */
#include "RAWUtils.h"
#include "TIFFUtil.h"
#include "FITSUtil.h"
#include "AHDDemosaicing.h"

#endif // _BITMAPEXT_H__
