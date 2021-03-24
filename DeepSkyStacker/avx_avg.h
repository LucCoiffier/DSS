#pragma once

#include "PixelTransform.h"
#include "StackingTasks.h"
#include "BackgroundCalibration.h"
#include "BitmapBase.h"
#include "avx_entropy.h"


class AvxAccumulation
{
	int resultWidth, resultHeight;
	CMemoryBitmap& tempBitmap;
	CMemoryBitmap& outputBitmap;
	const CTaskInfo& taskInfo;
	AvxEntropy& avxEntropy;
public:
	AvxAccumulation() = delete;
	AvxAccumulation(const CRect& resultRect, const CTaskInfo& tInfo, CMemoryBitmap& tempbm, CMemoryBitmap& outbm, AvxEntropy& entroinfo) noexcept;
	AvxAccumulation(const AvxAccumulation&) = delete;
	AvxAccumulation(AvxAccumulation&&) = delete;
	AvxAccumulation& AvxAccumulation::operator=(const AvxAccumulation&) = delete;

	int accumulate(const int nrStackedBitmaps);
private:
	template <class T_IN, class T_OUT>
	int doAccumulate(const int nrStackedBitmaps);
};
