#include <stdafx.h>
#include "EntropyInfo.h"
#include "DSSProgress.h"

/* ------------------------------------------------------------------- */

void CEntropyInfo::InitSquareEntropies()
{
	ZFUNCTRACE_RUNTIME();
	LONG						i, j;
	LONG						lSquareSize;

	lSquareSize = m_lWindowSize * 2 + 1;

	m_lNrSquaresX = m_pBitmap->Width() / lSquareSize;
	m_lNrSquaresY = m_pBitmap->Height() / lSquareSize;
	if (m_pBitmap->RealWidth() % lSquareSize)
		m_lNrSquaresX++;
	if (m_pBitmap->RealHeight() % lSquareSize)
		m_lNrSquaresY++;

	m_vRedEntropies.resize(m_lNrSquaresX*m_lNrSquaresY);
	m_vGreenEntropies.resize(m_lNrSquaresX*m_lNrSquaresY);
	m_vBlueEntropies.resize(m_lNrSquaresX*m_lNrSquaresY);

	if (m_pProgress)
		m_pProgress->Start2(NULL, m_lNrSquaresX);

	for (i = 0;i<m_lNrSquaresX;i++)
	{
		LONG			lMinX, 
						lMaxX;

		lMinX = i * lSquareSize;
		lMaxX = min((i+1) * lSquareSize -1, m_pBitmap->Width()-1);

		for (j = 0;j<m_lNrSquaresY;j++)
		{
			LONG		lMinY,
						lMaxY;
			double		fRedEntropy,
						fGreenEntropy,
						fBlueEntropy;

			lMinY = j * lSquareSize;
			lMaxY = min((j+1) * lSquareSize -1, m_pBitmap->Height()-1);
			// Compute the entropy for this square
			ComputeEntropies(lMinX, lMinY, lMaxX, lMaxY, fRedEntropy, fGreenEntropy, fBlueEntropy);

			m_vRedEntropies[i+j*m_lNrSquaresX]		= fRedEntropy;
			m_vGreenEntropies[i+j*m_lNrSquaresX]	= fGreenEntropy;
			m_vBlueEntropies[i+j*m_lNrSquaresX]		= fBlueEntropy;
		};

		if (m_pProgress)
			m_pProgress->Progress2(NULL, i+1);
	};
};

/* ------------------------------------------------------------------- */

void CEntropyInfo::ComputeEntropies(LONG lMinX, LONG lMinY, LONG lMaxX, LONG lMaxY, double & fRedEntropy, double & fGreenEntropy, double & fBlueEntropy)
{
	ZFUNCTRACE_RUNTIME();
	LONG						i, j;
	std::vector<WORD>			vRedHisto;
	std::vector<WORD>			vGreenHisto;
	std::vector<WORD>			vBlueHisto;
	LONG						lNrPixels;

	fRedEntropy = 0.0;
	fGreenEntropy = 0.0;
	fBlueEntropy = 0.0;

	lNrPixels = (lMaxX-lMinX+1)*(lMaxY-lMinY+1);
	vRedHisto.resize((LONG)MAXWORD+1);
	vGreenHisto.resize((LONG)MAXWORD+1);
	vBlueHisto.resize((LONG)MAXWORD+1);

	COLORREF16		crColor;
	for (i = lMinX;i<=lMaxX;i++)
	{
		for (j = lMinY;j<=lMaxY;j++)
		{
			m_pBitmap->GetPixel16(i, j, crColor);
			vRedHisto[crColor.red]++;
			vGreenHisto[crColor.green]++;
			vBlueHisto[crColor.blue]++;
		};
	};

	for (i = lMinX;i<=lMaxX;i++)
	{
		for (j = lMinY;j<=lMaxY;j++)
		{
			double			qRed,
							qBlue,
							qGreen;

			m_pBitmap->GetPixel16(i, j, crColor);

			qRed	= (double)vRedHisto[crColor.red]/(double)lNrPixels;
			qGreen	= (double)vGreenHisto[crColor.green]/(double)lNrPixels;
			qBlue	= (double)vBlueHisto[crColor.blue]/(double)lNrPixels;

			fRedEntropy += -qRed * log(qRed)/log(2.0);
			fGreenEntropy += -qGreen * log(qGreen)/log(2.0);
			fBlueEntropy += -qBlue * log(qBlue)/log(2.0);
		};
	};
};

/* ------------------------------------------------------------------- */
