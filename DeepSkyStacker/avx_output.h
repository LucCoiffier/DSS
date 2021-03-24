#pragma once

#include "BitmapBase.h"
#include <vector>


class AvxOutputComposition
{
private:
	CMultiBitmap& inputBitmap;
	CMemoryBitmap& outputBitmap;
	bool avxReady;
public:
	AvxOutputComposition() = delete;
	AvxOutputComposition(CMultiBitmap& mBitmap, CMemoryBitmap& outputbm);
	AvxOutputComposition(const AvxOutputComposition&) = delete;
	AvxOutputComposition(AvxOutputComposition&&) = delete;
	AvxOutputComposition& AvxOutputComposition::operator=(const AvxOutputComposition&) = delete;

	int compose(const int line, std::vector<void*> const& lineAddresses);
private:
	enum MethodSelection
	{
		KappaSigma = 0,
		MedianKappaSigma = 1,
		MedianOnly = 2
	};
	template <class INPUTTYPE, class OUTPUTTYPE>
	static bool bitmapColorOrGray(const CMultiBitmap& bitmap)  noexcept;

	template <class T>
	static float convertToFloat(const T value) noexcept;

	template <MethodSelection Method>
	int processMedianKappaSigma(const int line, std::vector<void*> const& lineAddresses);

	template <class T, MethodSelection Method>
	int doProcessMedianKappaSigma(const int line, std::vector<void*> const& lineAddresses);

	int processAutoAdaptiveWeightedAverage(const int line, std::vector<void*> const& lineAddresses);
	template <class T>
	int doProcessAutoAdaptiveWeightedAverage(const int line, std::vector<void*> const& lineAddresses);
};
