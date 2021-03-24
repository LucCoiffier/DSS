#pragma once

#include "avx_cfa.h"
#include "avx_entropy.h"
#include "PixelTransform.h"
#include "StackingTasks.h"
#include "BackgroundCalibration.h"
#include "BitmapBase.h"
#include <vector>
#include <tuple>


class AvxStacking
{
private:
	long lineStart, lineEnd, colEnd;
	int width, height;
	int resultWidth, resultHeight;
	std::vector<__m256> xCoordinates;
	std::vector<__m256> yCoordinates;
	std::vector<__m256> redPixels;
	std::vector<__m256> greenPixels;
	std::vector<__m256> bluePixels;
	CMemoryBitmap& inputBitmap;
	CMemoryBitmap& tempBitmap;
	AvxCfaProcessing avxCfa;
	AvxEntropy& entropyData;
public:
	AvxStacking() = delete;
	AvxStacking(long lStart, long lEnd, CMemoryBitmap& inputbm, CMemoryBitmap& tempbm, const CRect& resultRect, AvxEntropy& entrdat);
	AvxStacking(const AvxStacking&) = default;
	AvxStacking(AvxStacking&&) = delete;
	AvxStacking& operator=(const AvxStacking&) = delete;

	void init(const long lStart, const long lEnd);

	int stack(const CPixelTransform& pixelTransformDef, const CTaskInfo& taskInfo, const CBackgroundCalibration& backgroundCalibrationDef, const long pixelSizeMultiplier);
private:
	void resizeColorVectors(const size_t nrVectors);

	template <class T>
	int doStack(const CPixelTransform& pixelTransformDef, const CTaskInfo& taskInfo, const CBackgroundCalibration& backgroundCalibrationDef, const long pixelSizeMultiplier);

	int pixelTransform(const CPixelTransform& pixelTransformDef);

	template <class T, class LoopFunction, class InterpolParam>
	int backgroundCalibLoop(const LoopFunction& loopFunc, const class AvxSupport& avxSupport, const InterpolParam& redParams, const InterpolParam& greenParams, const InterpolParam& blueParams);

	template <class T>
	int backgroundCalibration(const CBackgroundCalibration& backgroundCalibrationDef);

	template <bool ISRGB, bool ENTROPY, class T>
	int pixelPartitioning();

	template <bool ISRGB>
	void getAvxEntropy(__m256& redEntropy, __m256& greenEntropy, __m256& blueEntropy, const __m256i xIndex, const int row);
};
