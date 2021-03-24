#ifndef __DSSTOOLS_H__
#define __DSSTOOLS_H__

#include <algorithm>
#include <numeric>
#include <float.h>
//#include <math.h>
#include <cmath>
#include "avx_median.h"

/* ------------------------------------------------------------------- */

inline bool	GetFileCreationDateTime(LPCTSTR szFileName, FILETIME & FileTime)
{
	bool			bResult = false;
	HANDLE			hFind;
	WIN32_FIND_DATA	FindData;

	hFind = FindFirstFile(szFileName, &FindData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		FileTime = FindData.ftLastWriteTime;
		FindClose(hFind);
		bResult = true;
	};

	return bResult;
};

/* ------------------------------------------------------------------- */

class CLinearInterpolation
{
private :
	double				xm,
						a0, b0,
						a1, b1;

	void	CopyFrom(const CLinearInterpolation & li)
	{
		xm = li.xm;
		a0 = li.a0;
		b0 = li.b0;
		a1 = li.a1;
		b1 = li.b1;
	};

public :
	CLinearInterpolation()
	{
		xm = 0;
		a0 = b0 = a1 = b1 = 0;
	};

	CLinearInterpolation(const CLinearInterpolation & li)
	{
		CopyFrom(li);
	};

	CLinearInterpolation & operator = (const CLinearInterpolation & li)
	{
		CopyFrom(li);
		return (*this);
	};

	double	Interpolate(double x)
	{
		if (x<xm)
			return a0*x+b0;
		else
			return a1*x+b1;
	};

	void	Initialize(double x0, double x1, double x2, double y0, double y1, double y2)
	{
		xm = x1;
		if (x0<x1)
			a0 = (y0-y1)/(x0-x1);
		else
			a0 = 0;

		b0 = y0 - a0*x0;

		if (x1 < x2)
			a1 = (y1-y2)/(x1-x2);
		else
			a1 = 0;
		b1 = y1 - a1*x1;
	};

	float getParameterXm() const { return static_cast<float>(this->xm); }
	float getParameterA0() const { return static_cast<float>(this->a0); }
	float getParameterA1() const { return static_cast<float>(this->a1); }
	float getParameterB0() const { return static_cast<float>(this->b0); }
	float getParameterB1() const { return static_cast<float>(this->b1); }
};

/*
	y = a0 x + b0 // when x < x1
	y = a1 x + b1 // when x >= x1

	y0 = a0 x0 + b0
	y1 = a0 x1 + b0

	a0 = (y0-y1)/(x0-x1)
	b0 = y0 - a0 x0

	y1 = a1 x1 + b1
	y2 = a1 x2 + b1

	a1 = (y1-y2)/(x1-x2)
	b1 = y1 - a1 x1
*/

/* ------------------------------------------------------------------- */

class CRationalInterpolation
{
private :
	double					a, b, c;
	double					fMin, fMax;

private :
	void	CopyFrom(const CRationalInterpolation & ri)
	{
		a = ri.a;
		b = ri.b;
		c = ri.c;
		fMin = ri.fMin;
		fMax = ri.fMax;
	};

public :
	CRationalInterpolation()
	{
		a = b = 0;
		c = 1;
        fMin = 0;
        fMax = 0;
	};

	CRationalInterpolation(const CRationalInterpolation & ri)
	{
		CopyFrom(ri);
	};

	CRationalInterpolation & operator = (const CRationalInterpolation & ri)
	{
		CopyFrom(ri);
		return (*this);
	};

	double	Interpolate(double x)
	{
		if (b || c)
			return std::max(std::min((x+a)/(b*x+c), fMax), fMin);
		else
			return std::max(std::min(x+a, fMax), fMin);
	};

	void	Initialize(double x0, double x1, double x2, double y0, double y1, double y2)
	{
		double				t1 = ((x0*y0 - x1*y1)*(y0-y2) - (x0*y0 - x2*y2)*(y0-y1));
		double				t2 = ((x0-x1)*(y0-y2) - (x0-x2)*(y0-y1));
		double				t3 = (y0-y1);

		if (t1)
			b = t2/t1;
		else
			b = 0;
		if (t3)
			c = ((x0-x1)-b*(x0*y0 - x1*y1))/t3;
		else
			c = 0;
		a = (b*x0 +c )*y0 - x0;

		fMin = std::min(std::min(y0, y1), y2);
		fMax = std::max(std::max(y0, y1), y2);
	};

	float getParameterA() const { return static_cast<float>(this->a); }
	float getParameterB() const { return static_cast<float>(this->b); }
	float getParameterC() const { return static_cast<float>(this->c); }
	float getParameterMin() const { return static_cast<float>(this->fMin); }
	float getParameterMax() const { return static_cast<float>(this->fMax); }
};

/*
(x0+a) = (b x0 +c )  y0;
(x1+a) = (b x1 +c )  y1;
(x2+a) = (b x2 +c )  y2;

x0-x1 = b (x0 y0 - x1 y1) + c(y0-y1)
x0-x2 = b (x0 y0 - x2 y2) + c(y0-y2)

(x0-x1)(y0-y2) - (x0-x2)(y0-y1) = b ((x0 y0 - x1 y1) (y0-y2) - (x0 y0 - x2 y2) (y0-y1))

b = ((x0-x1)(y0-y2) - (x0-x2)(y0-y1))/((x0 y0 - x1 y1) (y0-y2) - (x0 y0 - x2 y2) (y0-y1));
c = ((x0-x1)-b (x0 y0 - x1 y1))/(y0-y1);
a = (b x0 +c )  y0 - x0;
*/
/* ------------------------------------------------------------------- */

class CPointExt
{
public :
	double			X;
	double			Y;

private :
	void	CopyFrom(const CPointExt & pt)
	{
		X = pt.X;
		Y = pt.Y;
	};

public :
	CPointExt(double x = 0, double y = 0)
	{
		X = x;
		Y = y;
	};

	CPointExt(const CPointExt & pt)
	{
		CopyFrom(pt);
	};

	CPointExt(const CPoint & pt)
	{
		X = pt.x;
		Y = pt.y;
	};

	CPointExt(const CRect & rc)
	{
		X = (double)(rc.left+rc.right)/2.0;
		Y = (double)(rc.top+rc.bottom)/2.0;
	};

	CPointExt & operator = (const CPointExt & pt)
	{
		CopyFrom(pt);
		return (*this);
	};

	void	Offset(const CPointExt & pt)
	{
		X -= pt.X;
		Y -= pt.Y;
	};

	void	CopyTo(CPoint & pt)
	{
		pt.x = X;
		pt.y = Y;
	};

	bool operator != (const CPointExt & pt)
	{
		return (X != pt.X) || (Y!=pt.Y);
	};

	bool operator == (const CPointExt & pt)
	{
		return (X == pt.X) && (Y==pt.Y);
	};

	bool operator < (const CPointExt & pt)
	{
		return (X < pt.X);
	};

	bool	IsInRect(double fLeft, double fTop, double fRight, double fBottom)
	{
		return (X>=fLeft) && (X <= fRight) && (Y>=fTop) && (Y<=fBottom);
	};

	bool	IsNear(const CPointExt & ptTest)
	{
		return (fabs(X-ptTest.X) <= 3) && (fabs(Y-ptTest.Y) <= 3);
	};

	void Rotate(double fAngle, const CPointExt & ptCenter)
	{
		CPointExt		ptResult;
		double			fX, fY;

		ptResult.X = X - ptCenter.X;
		ptResult.Y = Y - ptCenter.Y;

		fX = cos(fAngle) * ptResult.X - sin(fAngle) * ptResult.Y;
		fY = sin(fAngle) * ptResult.X + cos(fAngle) * ptResult.Y;

		X = fX + ptCenter.X;
		Y = fY + ptCenter.Y;
	};
};

typedef std::vector<CPointExt>		POINTEXTVECTOR;
typedef POINTEXTVECTOR::iterator	POINTEXTITERATOR;

/* ------------------------------------------------------------------- */

inline double	Distance(const CPointExt & pt1, const CPointExt & pt2)
{
	return sqrt((pt1.X-pt2.X)*(pt1.X-pt2.X) + (pt1.Y-pt2.Y)*(pt1.Y-pt2.Y));
};

inline double	Distance(double fX1, double fY1, double fX2, double fY2)
{
	return sqrt((fX1-fX2)*(fX1-fX2) + (fY1-fY2)*(fY1-fY2));
};

/* ------------------------------------------------------------------- */

class	CDynamicStats
{
public :
	LONG			m_lNrValues;
	double			m_fSum;
	double			m_fPowSum;
	double			m_fMin,
					m_fMax;

public :
	CDynamicStats()
	{
		m_lNrValues = 0;
		m_fSum		= 0;
		m_fPowSum	= 0;
		m_fMin		= 0;
		m_fMax		= 0;
	};
	virtual ~CDynamicStats() {};

	void	AddValue(double fValue, LONG lNrValues = 1)
	{
		if (!m_lNrValues)
		{
			m_fMin = m_fMax = fValue;
		}
		else
		{
			m_fMin = std::min(m_fMin, fValue);
			m_fMax = std::max(m_fMax, fValue);
		};
		m_lNrValues+=lNrValues;
		m_fPowSum += (fValue*fValue)*lNrValues;
		m_fSum	  += fValue*lNrValues;
	};

	void	RemoveValue(double fValue, LONG lNrValues = 1)
	{
		m_lNrValues-=lNrValues;
		m_fPowSum -= (fValue*fValue)*lNrValues;
		m_fSum	  -= fValue*lNrValues;
	};

	double	Average()
	{
		if (m_lNrValues)
			return m_fSum/(double)m_lNrValues;
		else
			return 0;
	};

	double	Sigma()
	{
		if (m_lNrValues)
			return sqrt(m_fPowSum/m_lNrValues - pow(m_fSum/m_lNrValues, 2));
		else
			return 0;
	};

	double	Min()
	{
		return m_fMin;
	};

	double	Max()
	{
		return m_fMax;
	};
};

/* ------------------------------------------------------------------- */

inline double Median(double v1, double v2, double v3)
{
  if (v1 > v2)
  {
    if (v3 > v1)
		return v1;
    else if (v3 < v2)
		return v2;
    return v3;
  }
  else
  {
    if (v3 < v1)
		return v1;
    else if (v3 > v2)
		return v2;
    return v3;
  }
}

/* ------------------------------------------------------------------- */

template <class T> inline
double Median(std::vector<T>& values)
{
    if (values.empty())
        return 0;

    const int size = static_cast<int>(values.size());

	return qMedian(values.data(), size, size / 2);
/*
    // benchmarked: at around 40 elements, partial sort stars becoming faster
    // O(N) or O(2*N) for even count vs O(N*log(N))
    if (size > 40)
    {
        auto n = size / 2;
        std::nth_element(values.begin(), values.begin() + n, values.end());
        double median = values[n];

        if (size & 1) // odd
        {
            return median;
        }
        else // even
        {
            auto max = std::max_element(values.begin(), values.begin() + n);

            return (*max + median) / 2;
        }
    }
    else // fallback to classic sort
    {
        auto n = size / 2;
        std::sort(values.begin(), values.end());

        if (size & 1) // odd
        {
            return values[n];
        }
        else // even
        {
            return (values[n] + values[n - 1]) / 2;
        }
    }
*/
}

/* ------------------------------------------------------------------- */

template <class T> inline
T Maximum(const std::vector<T>& values)
{
    if (values.empty())
        return 0;

	return *std::max_element(values.begin(), values.end());
};

/* ------------------------------------------------------------------- */

template <class T> inline
double Average(const std::vector<T>& values)
{
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
};

/* ------------------------------------------------------------------- */

template <class T> inline
double Sigma2(const std::vector<T>& values, double& average)
{
	double result = 0.0;
	double squareDiff = 0.0;

	// Compute the average
	average = Average(values);

    for (double val : values)
        squareDiff += std::pow(val - average, 2);

    if (values.size())
        result = sqrt(squareDiff / values.size());

	return result;
};

/* ------------------------------------------------------------------- */

template <class T> inline
double Sigma(const std::vector<T>& values)
{
	double average;

	return Sigma2(values, average);
};

template <typename T>
double CalculateSigmaFromAverage(const std::vector<T>& values, const double targetAverage)
{
	double squareDiff = 0.0;

    for (double val : values)
        squareDiff += std::pow(val - targetAverage, 2);

	return sqrt(squareDiff / values.size());
}

/* ------------------------------------------------------------------- */

template <class T> inline
void	FillDynamicStat(const std::vector<T> & vValues, CDynamicStats & DynStats)
{
	for (LONG i = 0;i<vValues.size();i++)
		DynStats.AddValue(vValues[i]);
};

/* ------------------------------------------------------------------- */

class CFlatPart
{
public :
	LONG			m_lStart,
					m_lEnd;
	double			m_fAverage;
	double			m_fAverageVariation;
	double			m_fAbsAverageVariation;

private :
	void	CopyFrom(const CFlatPart & fp)
	{
		m_lStart			= fp.m_lStart;
		m_lEnd				= fp.m_lEnd;
		m_fAverage			= fp.m_fAverage;
		m_fAverageVariation = fp.m_fAverageVariation;
		m_fAbsAverageVariation = fp.m_fAbsAverageVariation;
	};

public :
	CFlatPart()
	{
		m_lStart = -1;
		m_lEnd	 = -1;
		m_fAverage = 0;
		m_fAverageVariation = 0;
		m_fAbsAverageVariation = 0;
	};
	~CFlatPart() {};
	CFlatPart(const CFlatPart & fp)
	{
		CopyFrom(fp);
	};

	CFlatPart & operator = (const CFlatPart & fp)
	{
		CopyFrom(fp);
		return (*this);
	};

	LONG	Length() const
	{
		return m_lEnd-m_lStart+1;
	};

	double	Score() const
	{
		if (m_fAbsAverageVariation)
			return (double)Length()/m_fAbsAverageVariation;//m_fAverageVariation;
		else
			return 0;
	};

	bool operator < (const CFlatPart & fp) const
	{
		return Score()>fp.Score();
		/*
		if (Length() > fp.Length())
			return true;
		else if (Length() < fp.Length())
			return false;
		else
			return m_fAverage < fp.m_fAverage;*/
	};
};

/* ------------------------------------------------------------------- */

template <class T> inline
void	DetectFlatParts(std::vector<T> & vValues, double fMaximum, std::vector<CFlatPart> & vFlatParts, double & fAverageVariation)
{
	double					fSummedVariation = 0;
	double					fAverage = 0;
	double					fTotalVariation = 0;

	bool					bInFlatPart = false;
	CFlatPart				fp;
	std::vector<double>		vVariations;
	std::vector<double>		vAbsVariations;

	fAverage = 0;
	fAverageVariation = 0;
	vVariations.reserve(vValues.size());
	vAbsVariations.reserve(vValues.size());

	for (LONG i = 0;i<vValues.size();i++)
	{
		if (!i)
		{
			vVariations.push_back(0);
			vAbsVariations.push_back(0);
		}
		else
		{
			vVariations.push_back((double)(vValues[i]-vValues[i-1])/(double)std::max(static_cast<T>(1), vValues[i-1])*sqrt(vValues[i]/fMaximum));
			vAbsVariations.push_back((double)(vValues[i]-vValues[i-1])/fMaximum);
		};
		fTotalVariation += fabs(vAbsVariations[i]);
		fAverage += vValues[i];
	};

	if (vValues.size())
	{
		fAverage /= vValues.size();
		fAverageVariation = fTotalVariation/vValues.size();
	};

	for (double fThreshold = 0.05;fThreshold<=0.20;fThreshold+=0.05)
	{
		fSummedVariation = 0;
		for (LONG i = 0;i<vValues.size();i++)
		{
			if (bInFlatPart)
			{
				if (fabs(fSummedVariation + vVariations[i])>fThreshold ||
					fabs(vAbsVariations[i])>0.01)
				{
					// End the flat part
					if (fp.m_lEnd-fp.m_lStart>2)
					{
						// This is a flat (at least 3 values)
						fp.m_fAverage /= fp.Length();
						fp.m_fAverageVariation /= fp.Length();
						fp.m_fAbsAverageVariation /= fp.Length();
						vFlatParts.push_back(fp);

						// Start again - a little later
						i = fp.m_lStart+1;
					};
					bInFlatPart = false;
				}
				else
				{
					// Continue the flat part
					fp.m_lEnd = i;
					fSummedVariation += vVariations[i];
					fp.m_fAverage    += vValues[i];
					fp.m_fAverageVariation += fabs(vVariations[i]);
					fp.m_fAbsAverageVariation += fabs(vAbsVariations[i]);
				};
			}
			if (!bInFlatPart)// && (fabs(vVariations[i])<=fThreshold))
			{
				// Start a new flat part
				fp.m_lStart = i;
				fp.m_lEnd   = i;
				fSummedVariation = 0;//vVariations[i];
				bInFlatPart = true;
				fp.m_fAverage			= vValues[i];
				fp.m_fAverageVariation  = 0;//fabs(vVariations[i]);
				fp.m_fAbsAverageVariation = 0;
			};
		};

		if (bInFlatPart && fp.Length())
		{
			fp.m_fAverage /= fp.Length();
			fp.m_fAverageVariation /= fp.Length();
			fp.m_fAbsAverageVariation /= fp.Length();
			vFlatParts.push_back(fp);
			bInFlatPart		 = false;
			fSummedVariation = 0;
		};
	};

	if (vValues.size())
	{
		// Check that at least one flat part is below the average
		bool				bFound = false;

		for (LONG i = 0;i<vFlatParts.size() && !bFound;i++)
			bFound = (vFlatParts[i].m_fAverage <= fAverage);
		for (LONG i = 0;i<vFlatParts.size() && bFound;i++)
		{
			if (vFlatParts[i].m_fAverage > fAverage)
			{
				// Remove this one
				vFlatParts[i].m_lEnd = vFlatParts[i].m_lStart-1;
			};
		};
	};
};

/* ------------------------------------------------------------------- */

template <class T> inline
T Minimum(std::vector<T>& values, bool ignoreZeros)
{
    T result = 0;

    if (!values.empty())
    {
        if (values.front() || !ignoreZeros)
            result = values.front();
    }

    for (T const& val : values)
        if (val || !ignoreZeros)
            result = std::min(result, val);

	return result;
};

/* ------------------------------------------------------------------- */

template <class T> inline
double	Homogenize(std::vector<T> & vValues, double fMaximum)
{
	double						fResult = 1.0;
	std::vector<CFlatPart> 		vFlatParts;
	double						fAverageVariation;

	DetectFlatParts(vValues, fMaximum, vFlatParts, fAverageVariation);

	std::sort(vFlatParts.begin(), vFlatParts.end());
	if (vFlatParts.size() && (vFlatParts[0].Length()>1))
	{
		// Keep only the longest part
		std::vector<T>			vAuxValues;

		vAuxValues.reserve(vFlatParts[0].Length());

		for (LONG i = vFlatParts[0].m_lStart;i<=vFlatParts[0].m_lEnd;i++)
			vAuxValues.push_back(vValues[i]);

		vValues = vAuxValues;
		//fAverageVariation = vFlatParts[0].m_fAbsAverageVariation;
	}
	else// if (fAverageVariation>0.05)
	{
		// Use only the minimum value for each channel
		T						fMinimum;

		fMinimum = Minimum(vValues, true);

		vValues.resize(1);		vValues[0] = fMinimum;
	};

	fResult = std::min(1.0, std::max(0.0, (1.05 - fAverageVariation*10)));

	return fResult;
};

/* ------------------------------------------------------------------- */

template <class T> inline
double Homogenize3(std::vector<T> & vValues, LONG lNrSubStacks)
{
	double						fResult = 1.0;
	std::vector<double>			vStackValues;
	std::vector<T>				vWork;
	std::vector<T>				vSubStack;
	LONG						lNrValues = vValues.size();

	vStackValues.reserve(lNrSubStacks);
	vSubStack.reserve(lNrValues/lNrSubStacks+1);
	vWork.reserve(lNrValues / lNrSubStacks + 1);

	for (LONG j = 0;j<lNrSubStacks;j++)
	{
		vSubStack.clear();
		for (LONG i = j;i<lNrValues;i+=lNrSubStacks)
			vSubStack.push_back(vValues[i]);

		vStackValues.push_back(KappaSigmaClip(vSubStack, 1.5, 3, vWork));
	};

	fResult = Median(vStackValues);
	vValues.clear();
	vValues.push_back(fResult);

	return fResult;
};

/* ------------------------------------------------------------------- */

template <class T> inline
double Homogenize3(std::vector<T> & v1Values, std::vector<T> & v2Values, std::vector<T> & v3Values, LONG lNrSubStacks)
{
	Homogenize3(v1Values, lNrSubStacks);
	Homogenize3(v2Values, lNrSubStacks);
	Homogenize3(v3Values, lNrSubStacks);

	return 1.0;
};

/* ------------------------------------------------------------------- */

template <class T> inline
double	Homogenize(std::vector<T> & v1Values, std::vector<T> & v2Values, std::vector<T> & v3Values, double fMaximum)
{
	double						fResult = 1.0;
	std::vector<double>			vValues;

	vValues.reserve(v1Values.size());

	for (LONG i = 0;i<v1Values.size();i++)
		vValues.push_back(double(v1Values[i]+v2Values[i]+v3Values[i])/3.0);

/*
	std::vector<CFlatPart> 		v1FlatParts;
	std::vector<CFlatPart> 		v2FlatParts;
	std::vector<CFlatPart> 		v3FlatParts;
	double						f1AverageVariation;
	double						f2AverageVariation;
	double						f3AverageVariation;

	DetectFlatParts(v1Values, fMaximum, v1FlatParts, f1AverageVariation);
	DetectFlatParts(v2Values, fMaximum, v2FlatParts, f2AverageVariation);
	DetectFlatParts(v3Values, fMaximum, v3FlatParts, f3AverageVariation);

	std::vector<CFlatPart> 		vFlatParts;

	vFlatParts.reserve(v1FlatParts.size()+v2FlatParts.size()+v3FlatParts.size());
	vFlatParts = v1FlatParts;
	for (LONG i = 0;i<v2FlatParts.size();i++)
		vFlatParts.push_back(v2FlatParts[i]);
	for (LONG i = 0;i<v3FlatParts.size();i++)
		vFlatParts.push_back(v3FlatParts[i]);
*/
	std::vector<CFlatPart> 		vFlatParts;
	double						fAverageVariation;

	DetectFlatParts(vValues, fMaximum, vFlatParts, fAverageVariation);

	std::sort(vFlatParts.begin(), vFlatParts.end());
	if (vFlatParts.size() && (vFlatParts[0].Length()>1))
	{
		// Keep only the longest part
		std::vector<T>			v1AuxValues;
		std::vector<T>			v2AuxValues;
		std::vector<T>			v3AuxValues;

		v1AuxValues.reserve(vFlatParts[0].Length());
		v2AuxValues.reserve(vFlatParts[0].Length());
		v3AuxValues.reserve(vFlatParts[0].Length());

		for (LONG i = vFlatParts[0].m_lStart;i<=vFlatParts[0].m_lEnd;i++)
		{
			v1AuxValues.push_back(v1Values[i]);
			v2AuxValues.push_back(v2Values[i]);
			v3AuxValues.push_back(v3Values[i]);
		};

		v1Values = v1AuxValues;
		v2Values = v2AuxValues;
		v3Values = v3AuxValues;
	}
	else if (fAverageVariation>0.05)
	{
		// Use only the minimum value for each channel
		T						f1Minimum,
								f2Minimum,
								f3Minimum;

		f1Minimum = Minimum(v1Values, true);
		f2Minimum = Minimum(v2Values, true);
		f3Minimum = Minimum(v3Values, true);

		v1Values.resize(1);		v1Values[0] = f1Minimum;
		v2Values.resize(1);		v2Values[0] = f2Minimum;
		v3Values.resize(1);		v3Values[0] = f3Minimum;
	};

	return fResult;
};

/* ------------------------------------------------------------------- */

template <class T> inline
void	Homogenize2(std::vector<T> & vValues, double fMaximum)
{
	if (vValues.size()>3)
	{
		bool				bEnd = false;
		LONG				i;

		std::sort(vValues.begin(), vValues.end());
		// Compute the distance between the line and each point
		if (vValues[0] != vValues[vValues.size()-1])
		{
			double			fMin  = vValues[0],
							fMax  = vValues[vValues.size()-1];
			double			fSteep = (fMax-fMin)/vValues.size();
			double			fMaxDistance = 0;
			LONG			lIndice1;

			for (i = 0;i<vValues.size();i++)
			{
				double		fDistance;

				fDistance = (fSteep * i - (vValues[i]-fMin))/(fMax-fMin);
				if (fDistance>fMaxDistance)
				{
					fMaxDistance = fDistance;
					lIndice1 = i;
				};
			};
			if (fMaxDistance<0.2)
				lIndice1 = -1;

			// Compute the second indice based on the variation
			// between the minimum and the current value
			LONG			lIndice2 = -1;
			for (i = 0;i<vValues.size() && (lIndice2<0);i++)
			{
				double		fIncrease;

				fIncrease = (vValues[i]-fMin)/vValues[i]/sqrt(fMin/fMaximum);
				if (fIncrease>0.15)
					lIndice2 = i;
			};

			if (lIndice1>=0 || lIndice2>=0)
			{
				// Cut at this position
				LONG		lIndice = std::min(lIndice1==-1 ? 10000 : lIndice1,
										  lIndice2==-1 ? 10000 : lIndice2);
				vValues.resize(lIndice+1);
			}

			// Now analyze the distribution for its homogeneity
			//std::vector<double>		vAuxValues;
			fMax = vValues[vValues.size()-1];

			if (fMax>fMin)
			{

				/*vAuxValues.resize(vValues.size());
				for (i = 0;i<vAuxValues.size();i++)
					vAuxValues[i] = (vValues[i]-fMin)/(fMax-fMin);*/

				double		fSigma,
							fAverage;

				fSigma = Sigma2(vValues, fAverage);

				if (fSigma/fAverage>0.15) // Discard the whole set
					vValues.clear();
			};
		};
	};
};

/* ------------------------------------------------------------------- */

template <class T> inline
double	KappaSigmaClip(const std::vector<T> & vValues, double fKappa, LONG lIteration, std::vector<T> & vAuxValues)
{
	double			Result = 0;
	bool			bEnd = false;
	CDynamicStats	DynStats;

	//
	// Copy the data
	//
	vAuxValues = vValues;

	std::sort(vAuxValues.begin(), vAuxValues.end());

	FillDynamicStat(vAuxValues, DynStats);

	for (LONG i = 0;i<lIteration && !bEnd;i++)
	{
		double			fAverage;
		double			fSigma;
		LONG			lCurrentIndice = 0;
		LONG			j = 0;
		double			fMin,
						fMax;

		fSigma	 = DynStats.Sigma(); //Sigma2(vAuxValues, fAverage);
		fAverage = DynStats.Average();

		fMin = fAverage - fKappa * fSigma;
		fMax = fAverage + fKappa * fSigma;

		while (j<vAuxValues.size() && vAuxValues[j]<fMin)
		{
			DynStats.RemoveValue(vAuxValues[j]);
			j++;
		};
		while (j<vAuxValues.size() && vAuxValues[j]<=fMax)
		{
			vAuxValues[lCurrentIndice] = vAuxValues[j];
			lCurrentIndice++;
			j++;
		};

		while (j<vAuxValues.size())
		{
			DynStats.RemoveValue(vAuxValues[j]);
			j++;
		};

/*		for (LONG j = 0;j<vAuxValues.size();j++)
		{
			if (((double)vAuxValues[j]>= (fAverage - fKappa*fSigma)) &&
				((double)vAuxValues[j]<= (fAverage + fKappa*fSigma)))
				vTempValues.push_back(vAuxValues[j]);
			else
				DynStats.RemoveValue(vAuxValues[j]);
		};*/

		bEnd = !lCurrentIndice || (lCurrentIndice == vAuxValues.size());
		vAuxValues.resize(lCurrentIndice);
		//bEnd = !vTempValues.size() || (vAuxValues.size() == vTempValues.size());
		//vAuxValues = vTempValues;
	};

	Result = DynStats.Average();//Average(vAuxValues);

	return Result;
};

/* ------------------------------------------------------------------- */

template <class T> inline
double	MedianKappaSigmaClip(const std::vector<T> & vValues, double fKappa, LONG lIteration, std::vector<T>& vWorkingBuffer1, std::vector<T>& vWorkingBuffer2)
{
	double			Result = 0;

	// Set up the working buffers - we can flip between them to prevent
	// needless copying of vectors.
	vWorkingBuffer1.clear();
	vWorkingBuffer2.clear();

	std::vector<T>& vecCurrentPass = vWorkingBuffer1;
	std::vector<T>& vecTempBuffer = vWorkingBuffer2;

	// Initial copy into the working set to start us off.
	vWorkingBuffer1 = vValues;
	for (LONG i = 0;i<lIteration;i++)
	{
		double			fAverage;
		double			fSigma;
		T				fMedian;

		fMedian = Median(vecCurrentPass);
		fSigma = Sigma2(vecCurrentPass, fAverage);

		// Go through and populate the temp buffer according to the values.
		vecTempBuffer.clear();
		for (LONG j = 0;j< vecCurrentPass.size();j++)
		{
			if (((double)vecCurrentPass[j]>= (fAverage - fKappa*fSigma)) &&
				((double)vecCurrentPass[j]<= (fAverage + fKappa*fSigma)))
				vecTempBuffer.push_back(vecCurrentPass[j]);
			else
				vecTempBuffer.push_back(fMedian);
		}

		// Swap temp and working buffers for next pass.
		std::vector<T>& vecTmp = vecCurrentPass;
		vecCurrentPass = vecTempBuffer;
		vecTempBuffer = vecTmp;
	}

	// The final pass will now be in the current buffer (because of the swap at the end of the loop).
	Result = Average(vecCurrentPass);

	return Result;
};

/* ------------------------------------------------------------------- */

template <typename T>
double AutoAdaptiveWeightedAverage(const std::vector<T> & vValues, long lIterations, std::vector<double> & vWeights)
{
	// Computes the auto-adaptive weighted average of a set of numbers
	// (intended to be the values of the same pixel in different stacked images).
	//
	// Based on pp. 44-51 of:
	//
	// Peter B. Stetson (1989)
	//	"The Techniques of Least Squares and Stellar Photometry with CCDs"
	//
	// Which was presented at:
	//	V Escola Avançada de Astrofísica,
	//	Aguas de São Pedro, Brazil
	//
	// and is archived online at:
	//	https://ned.ipac.caltech.edu/level5/Stetson/Stetson_contents.html
	//	(section "Non-Gaussian Error Distributions")
	//
	// This was further interpreted (maybe changed) by an anonymous author at
	// https://archive.stsci.edu/hst/wfpc2/pipeline.html
	// (this author puts in a 1/sigma2 term that is constant and should have
	// no effect on normalized weights; Stetson's sigma is the sigma of the data
	// whereas the second author's sigma2 is "derived from read noise and gain").
	//
	// I am indebted to Michael A. Covington and Simon C. Smith for their
	// immense help in getting this working.  Much of the code is adapted from
	// Simon's code simply because it was C++ whereas Michael's written in C#
	//
	// Regardless any faults are my own!
	// David C. Partridge
	// 29 July 2019
	//

	double			fResult = 0;
	size_t			i = 0;
	size_t			nElements = vValues.size();

	// If the standard deviation is less than (say) five there's not much point iterating
	// the auto-adaptive average calculation, in that case just return a regular average.
	const double	SMALL = 5.0;

	//
	// Probably not too bright an idea to do weighted average on a sample size less than 3
	//
	if (nElements > 2)
	{
		double		fMaximum = vValues[0];

		for (i = 1; i < nElements; i++)
			fMaximum = std::max(fMaximum, (double)vValues[i]);

		if (fMaximum > 0)
		{
			vWeights.resize(nElements);

			// Start with the mean value of the set.
			double runningAverage = Average(vValues);
			double fSigma = 0.0;

			// Calculate weights for the values, but re-iterate with the
			// new "weighted" average each time. This should converge on
			// a single value over time. Rumour has it that 5 will do :)
			for (int nIteration = 0; nIteration < (int)lIterations; nIteration++)
			{
				//
				// Calculate the standard deviation from the current running average.
				//
				fSigma = CalculateSigmaFromAverage(vValues, runningAverage);

				//
				// Escape clause for small sigma - where all input data
				// are the same or darn nearly so.
				//
				if (0 == nIteration && fSigma <= SMALL) return runningAverage;

				//
				// Calculate the weights
				//
				for (size_t j = 0; j != nElements; j++)
				{
					const double r = fabs(vValues[j] - runningAverage);
					//
					// The 1/sigma^2 term is ignored as it will be constant
					// for all weights.
					//
					vWeights[j] = 1 / (1 + ((r / fSigma) * (r / fSigma)));
				}
				const double weightTotal = std::accumulate(vWeights.begin(), vWeights.end(), 0.0);

				// Calculate the new running average.
				runningAverage = 0.0;
				for (size_t nIndex = 0; nIndex < nElements; nIndex++)
					runningAverage += (vValues[nIndex] * vWeights[nIndex]) / weightTotal;
			}

			//
			// If we didn't already return because of small sigma, then the result is in
			// variable runningAverage
			//
			fResult = runningAverage;
		};
	}
	else
		fResult = Average(vValues);

	return fResult;
};

/* ------------------------------------------------------------------- */

typedef enum TRANSFORMATIONTYPE
{
	TT_LINEAR		= 0,
	TT_BILINEAR		= 1,
	TT_BISQUARED	= 2,
	TT_BICUBIC		= 3,
	TT_NONE			= 4,
	TT_LAST			= 5
}TRANSFORMATIONTYPE;

class CBilinearParameters
{
public :
	TRANSFORMATIONTYPE		Type;
	double					a0, a1, a2, a3;
	double					a4, a5, a6, a7, a8;
	double					a9, a10, a11, a12, a13, a14, a15;
	double					b0, b1, b2, b3;
	double					b4, b5, b6, b7, b8;
	double					b9, b10, b11, b12, b13, b14, b15;

	double					fXWidth,
							fYWidth;

private :
	void	CopyFrom(const CBilinearParameters & bp)
	{
		Type    = bp.Type;
		a0 = bp.a0;
		a1 = bp.a1;
		a2 = bp.a2;
		a3 = bp.a3;
		a4 = bp.a4;
		a5 = bp.a5;
		a6 = bp.a6;
		a7 = bp.a7;
		a8 = bp.a8;
		a9 = bp.a9;
		a10 = bp.a10;
		a11 = bp.a11;
		a12 = bp.a12;
		a13 = bp.a13;
		a14 = bp.a14;
		a15 = bp.a15;

		b0 = bp.b0;
		b1 = bp.b1;
		b2 = bp.b2;
		b3 = bp.b3;
		b4 = bp.b4;
		b5 = bp.b5;
		b6 = bp.b6;
		b7 = bp.b7;
		b8 = bp.b8;
		b9 = bp.b9;
		b10 = bp.b10;
		b11 = bp.b11;
		b12 = bp.b12;
		b13 = bp.b13;
		b14 = bp.b14;
		b15 = bp.b15;

		fXWidth = bp.fXWidth;
		fYWidth = bp.fYWidth;
	};

private :
	bool	GetNextParameter(CString & strParameters, double & fValue)
	{
		bool			bResult = false;
		int				nPos;
		CString			strValue;

		if (strParameters.GetLength())
		{
			nPos = strParameters.Find(_T(","));
			if (nPos>=0)
				strValue = strParameters.Left(nPos);
			else
				strValue = strParameters;

			fValue = _ttof((LPCTSTR)strValue);	// Change _ttof to _ttof for Unicode
			strParameters = strParameters.Right(std::max(0, strParameters.GetLength()-strValue.GetLength()-1));
			bResult = true;
		};

		return bResult;
	};

public :
	CBilinearParameters()
	{
		Clear();
	};

	CBilinearParameters(const CBilinearParameters & bp)
	{
		CopyFrom(bp);
	};

	const CBilinearParameters & operator = (const CBilinearParameters & bp)
	{
		CopyFrom(bp);
		return (*this);
	};

	void	Clear()
	{
		Type = TT_BILINEAR;
		a0 = a1 = a2 = a3 = a4 = a5 = a6 = a7 = a8 = 0.0;
		a9 = a10 = a11 = a12 = a13 = a14 = a15 = 0.0;
		b0 = b1 = b2 = b3 = b4 = b5 = b6 = b7 = b8 = 0.0;
		b9 = b10 = b11 = b12 = b13 = b14 = b15 = 0.0;
		a1 = 1.0;	// to have x' = x
		b2 = 1.0;	// to have y' = y

		fXWidth = fYWidth = 1.0;
	};

	void	ToText(CString & strText)
	{
		if (Type == TT_NONE)
		{
			strText.Format(_T("None(%.20g,%.20g)"), fXWidth, fYWidth);
		}
		else if (Type == TT_BICUBIC)
		{
			CString			strText1;

			strText1.Format(_T("Bicubic(%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,"),
										a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15);
			strText = strText1;

			strText1.Format(_T("%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,"),
										b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15);
			strText += strText1;
			strText1.Format(_T("%.20g,%.20g)"),
										fXWidth, fYWidth);
			strText += strText1;
		}
		else if (Type == TT_BISQUARED)
		{
			CString			strText1;

			strText1.Format(_T("Bisquared(%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,"),
										a0, a1, a2, a3, a4, a5, a6, a7, a8);
			strText = strText1;

			strText1.Format(_T("%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g)"),
										b0, b1, b2, b3, b4, b5, b6, b7, b8,
										fXWidth, fYWidth);
			strText += strText1;
		}
		else
		{
			strText.Format(_T("Bilinear(%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g,%.20g)"),
										a0, a1, a2, a3,
										b0, b1, b2, b3,
										fXWidth, fYWidth);
		};
	};

	bool	FromText(LPCTSTR szText)
	{
		bool			bResult = false;
		CString			strText = szText;
		int				nPos1,
						nPos2;

		nPos1 = strText.Find(_T("("));
		nPos2 = strText.Find(_T(")"));
		if (nPos1>=0 && nPos2>nPos1)
		{
			CString		strType;
			CString		strParameters;

			strType			= strText.Left(nPos1);
			strParameters	= strText.Mid(nPos1+1, nPos2-nPos1-1);
			if (!strType.CompareNoCase(_T("None")))
			{
				Clear();
				Type = TT_NONE;
				bResult = GetNextParameter(strParameters, fXWidth) &&
						  GetNextParameter(strParameters, fYWidth);
			}
			else if (!strType.CompareNoCase(_T("Bilinear")))
			{
				Type = TT_BILINEAR;
				bResult = GetNextParameter(strParameters, a0) &&
						  GetNextParameter(strParameters, a1) &&
						  GetNextParameter(strParameters, a2) &&
						  GetNextParameter(strParameters, a3) &&
						  GetNextParameter(strParameters, b0) &&
						  GetNextParameter(strParameters, b1) &&
						  GetNextParameter(strParameters, b2) &&
						  GetNextParameter(strParameters, b3) &&
						  GetNextParameter(strParameters, fXWidth) &&
						  GetNextParameter(strParameters, fYWidth);
			}
			else if (!strType.CompareNoCase(_T("Bisquared")))
			{
				Type = TT_BISQUARED;
				bResult = GetNextParameter(strParameters, a0) &&
						  GetNextParameter(strParameters, a1) &&
						  GetNextParameter(strParameters, a2) &&
						  GetNextParameter(strParameters, a3) &&
						  GetNextParameter(strParameters, a4) &&
						  GetNextParameter(strParameters, a5) &&
						  GetNextParameter(strParameters, a6) &&
						  GetNextParameter(strParameters, a7) &&
						  GetNextParameter(strParameters, a8) &&
						  GetNextParameter(strParameters, b0) &&
						  GetNextParameter(strParameters, b1) &&
						  GetNextParameter(strParameters, b2) &&
						  GetNextParameter(strParameters, b3) &&
						  GetNextParameter(strParameters, b4) &&
						  GetNextParameter(strParameters, b5) &&
						  GetNextParameter(strParameters, b6) &&
						  GetNextParameter(strParameters, b7) &&
						  GetNextParameter(strParameters, b8) &&
						  GetNextParameter(strParameters, fXWidth) &&
						  GetNextParameter(strParameters, fYWidth);
			}
			else if (!strType.CompareNoCase(_T("Bicubic")))
			{
				Type = TT_BICUBIC;
				bResult = GetNextParameter(strParameters, a0) &&
						  GetNextParameter(strParameters, a1) &&
						  GetNextParameter(strParameters, a2) &&
						  GetNextParameter(strParameters, a3) &&
						  GetNextParameter(strParameters, a4) &&
						  GetNextParameter(strParameters, a5) &&
						  GetNextParameter(strParameters, a6) &&
						  GetNextParameter(strParameters, a7) &&
						  GetNextParameter(strParameters, a8) &&
						  GetNextParameter(strParameters, a9) &&
						  GetNextParameter(strParameters, a10) &&
						  GetNextParameter(strParameters, a11) &&
						  GetNextParameter(strParameters, a12) &&
						  GetNextParameter(strParameters, a13) &&
						  GetNextParameter(strParameters, a14) &&
						  GetNextParameter(strParameters, a15) &&
						  GetNextParameter(strParameters, b0) &&
						  GetNextParameter(strParameters, b1) &&
						  GetNextParameter(strParameters, b2) &&
						  GetNextParameter(strParameters, b3) &&
						  GetNextParameter(strParameters, b4) &&
						  GetNextParameter(strParameters, b5) &&
						  GetNextParameter(strParameters, b6) &&
						  GetNextParameter(strParameters, b7) &&
						  GetNextParameter(strParameters, b8) &&
						  GetNextParameter(strParameters, b9) &&
						  GetNextParameter(strParameters, b10) &&
						  GetNextParameter(strParameters, b11) &&
						  GetNextParameter(strParameters, b12) &&
						  GetNextParameter(strParameters, b13) &&
						  GetNextParameter(strParameters, b14) &&
						  GetNextParameter(strParameters, b15) &&
						  GetNextParameter(strParameters, fXWidth) &&
						  GetNextParameter(strParameters, fYWidth);
			};
		};

		return bResult;
	};

	CPointExt Transform(const CPointExt & pt) const
	{
		CPointExt	ptResult;

		if (Type == TT_BICUBIC)
		{
			double			X = pt.X/fXWidth;
			double			X2 = X * X;
			double			X3 = X * X * X;
			double			Y = pt.Y/fYWidth;
			double			Y2 = Y * Y;
			double			Y3 = Y * Y * Y;

			ptResult.X = a0 + a1 * X + a2 * Y + a3 * X * Y
						+ a4 * X2 + a5 * Y2 + a6 * X2 * Y + a7 * X * Y2 + a8 * X2 * Y2
						+ a9 * X3 + a10 * Y3 + a11 * X3 * Y + a12 * X * Y3 + a13 * X3 * Y2 + a14 * X2 * Y3 + a15 * X3 * Y3;
			ptResult.Y = b0 + b1 * X + b2 * Y + b3 * X * Y
						+ b4 * X2 + b5 * Y2 + b6 * X2 * Y + b7 * X * Y2 + b8 * X2 * Y2
						+ b9 * X3 + b10 * Y3 + b11 * X3 * Y + b12 * X * Y3 + b13 * X3 * Y2 + b14 * X2 * Y3 + b15 * X3 * Y3;
		}
		else if (Type == TT_BISQUARED)
		{
			double			X = pt.X/fXWidth;
			double			X2 = X * X;
			double			Y = pt.Y/fYWidth;
			double			Y2 = Y * Y;

			ptResult.X = a0 + a1 * X + a2 * Y + a3 * X * Y
						+ a4 * X2 + a5 * Y2 + a6 * X2 * Y + a7 * X * Y2 + a8 * X2 * Y2;
			ptResult.Y = b0 + b1 * X + b2 * Y + b3 * X * Y
						+ b4 * X2 + b5 * Y2 + b6 * X2 * Y + b7 * X * Y2 + b8 * X2 * Y2;
		}
		else
		{
			double			X = pt.X / fXWidth;
			double			Y = pt.Y / fYWidth;

			ptResult.X = a0 + a1 * X + a2 * Y + a3 * X * Y;
			ptResult.Y = b0 + b1 * X + b2 * Y + b3 * X * Y;
		};

		ptResult.X *= fXWidth;
		ptResult.Y *= fYWidth;

		return ptResult;
	};

	double	Angle(LONG lWidth) const
	{
		double		fAngle;
		CPointExt	pt1 (0, 0),
					pt2 (lWidth, 0);

		pt1 = Transform(pt1);
		pt2 = Transform(pt2);

		fAngle = atan2(pt2.Y - pt1.Y, pt2.X - pt1.X);

		return fAngle;
	};

	void	Offsets(double & dX, double & dY)
	{
		dX = a0 * fXWidth;
		dY = b0 * fYWidth;
	};

	void	Footprint(CPointExt & pt1, CPointExt & pt2, CPointExt & pt3, CPointExt & pt4)
	{
		pt1.X = pt1.Y = 0;
		pt2.X = fXWidth;	pt2.Y = 0;
		pt3.X = fXWidth;	pt3.Y = fYWidth;
		pt4.X = 0;			pt4.Y = fYWidth;

		pt1 = Transform(pt1);
		pt2 = Transform(pt2);
		pt3 = Transform(pt3);
		pt4 = Transform(pt4);
	};
};

#endif // __DSSTOOLS_H__
