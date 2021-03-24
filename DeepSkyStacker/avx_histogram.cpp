#include "StdAfx.h"
#include "avx_histogram.h"
#include "avx_cfa.h"
#include <immintrin.h>

AvxHistogram::AvxHistogram(CMemoryBitmap& inputbm) :
	redHisto(HistogramSize(), 0),
	greenHisto{},
	blueHisto{},
	inputBitmap{ inputbm },
	avxReady{ AvxSupport::checkSimdAvailability() },
	allRunsSuccessful{ true }
{
	if (avxReady && AvxSupport{ inputBitmap }.isColorBitmapOrCfa())
	{
		greenHisto.resize(HistogramSize(), 0);
		blueHisto.resize(HistogramSize(), 0);
	}

	static_assert(sizeof(HistogramVectorType::value_type) == sizeof(int));
}

int AvxHistogram::calcHistogram(const size_t lineStart, const size_t lineEnd)
{
	const auto rval = [this](const int rv) -> int
	{
		if (rv != 0)
			this->allRunsSuccessful = false;
		return rv;
	};

	if (!avxReady)
		return rval(1);

	int rv = 1;
	if (doCalcHistogram<WORD>(lineStart, lineEnd) == 0
		|| doCalcHistogram<unsigned long>(lineStart, lineEnd) == 0
		|| doCalcHistogram<float>(lineStart, lineEnd) == 0
		|| doCalcHistogram<double>(lineStart, lineEnd) == 0)
	{
		rv = 0;
	}
	return AvxSupport::zeroUpper(rval(rv));
}

template <class T>
int AvxHistogram::doCalcHistogram(const size_t lineStart, const size_t lineEnd)
{
	// Check input bitmap.
	const AvxSupport avxInputSupport{ inputBitmap };
	if (!avxInputSupport.isColorBitmapOfType<T>() && !avxInputSupport.isMonochromeBitmapOfType<T>()) // Monochrome includes CFA
		return 1;

	constexpr size_t vectorLen = 16;
	const size_t width = inputBitmap.Width();
	const size_t nrVectors = width / vectorLen;

	// AVX makes no sense for super-small arrays.
	if (width < 256 || inputBitmap.Height() < 32)
		return 1;

	const auto calcHistoOfTwoVectorsEpi32 = [](const std::tuple<__m256i, __m256i>& twoVectors, auto& histogram) -> void
	{
		const auto [lo, hi] = twoVectors;
		calcHistoOfVectorEpi32(lo, histogram);
		calcHistoOfVectorEpi32(hi, histogram);
	};

	const bool isCFA = avxInputSupport.isMonochromeCfaBitmapOfType<T>();

	// Color bitmap (incl. CFA)
	// ------------------------
	if (avxInputSupport.isColorBitmapOfType<T>() || isCFA)
	{
		if constexpr (std::is_same<T, double>::value) // color-double not supported.
			return 1;

		AvxCfaProcessing avxCfa{ 0, 0, inputBitmap };
		if (isCFA)
		{
			avxCfa.init(lineStart, lineEnd);
			avxCfa.interpolate(lineStart, lineEnd, 1);
		}

		for (size_t row = lineStart, lineNdx = 0; row < lineEnd; ++row, ++lineNdx)
		{
			const T* pRedPixels = isCFA ? avxCfa.redCfaLine<T>(lineNdx) : &avxInputSupport.redPixels<T>().at(row * width);
			const T* pGreenPixels = isCFA ? avxCfa.greenCfaLine<T>(lineNdx) : &avxInputSupport.greenPixels<T>().at(row * width);
			const T* pBluePixels = isCFA ? avxCfa.blueCfaLine<T>(lineNdx) : &avxInputSupport.bluePixels<T>().at(row * width);

			for (size_t counter = 0; counter < nrVectors; ++counter, pRedPixels += vectorLen, pGreenPixels += vectorLen, pBluePixels += vectorLen)
			{
				calcHistoOfTwoVectorsEpi32(AvxSupport::read16PackedInt(pRedPixels), redHisto);
				calcHistoOfTwoVectorsEpi32(AvxSupport::read16PackedInt(pGreenPixels), greenHisto);
				calcHistoOfTwoVectorsEpi32(AvxSupport::read16PackedInt(pBluePixels), blueHisto);
			}
			for (size_t n = nrVectors * vectorLen; n < width; ++n, ++pRedPixels, ++pGreenPixels, ++pBluePixels)
			{
				addToHisto(redHisto, *pRedPixels);
				addToHisto(greenHisto, *pGreenPixels);
				addToHisto(blueHisto, *pBluePixels);
			}
		}
		return 0;
	}

	// Note:
	// Gray input bitmaps of type double use a fix scaling factor of 256.
	// This is for the histogram in the registering process, where the color values come from the luminance calculation. They are in the range [0, 256).
	// Thus, they need up-scaling by a factor of 256.
	if (avxInputSupport.isMonochromeBitmapOfType<T>())
	{
		for (size_t row = lineStart; row < lineEnd; ++row)
		{
			const T* pGrayPixels = &avxInputSupport.grayPixels<T>().at(row * width);
			for (size_t counter = 0; counter < nrVectors; ++counter, pGrayPixels += vectorLen)
			{
				if constexpr (std::is_same<T, double>::value)
					calcHistoOfTwoVectorsEpi32(AvxSupport::read16PackedInt(pGrayPixels, _mm256_set1_pd(256.0)), redHisto);
				else
					calcHistoOfTwoVectorsEpi32(AvxSupport::read16PackedInt(pGrayPixels), redHisto);
			}
			for (size_t n = nrVectors * vectorLen; n < width; ++n, ++pGrayPixels)
			{
				addToHisto(redHisto, *pGrayPixels);
			}
		}
		return 0;
	}

	return 1;
}

int AvxHistogram::mergeHistograms(HistogramVectorType& red, HistogramVectorType& green, HistogramVectorType& blue)
{
	if (!avxReady)
		return 1;

	const auto mergeHisto = [](HistogramVectorType& targetHisto, const HistogramVectorType& sourceHisto) -> void
	{
		if (targetHisto.size() == HistogramSize() && sourceHisto.size() == HistogramSize())
		{
			constexpr size_t nrVectors = HistogramSize() / 8;
			auto* pTarget{ &*targetHisto.begin() };
			const auto* pSource{ &*sourceHisto.begin() };

			for (size_t n = 0; n < nrVectors; ++n, pTarget += 8, pSource += 8)
			{
				const __m256i tgt = _mm256_add_epi32(_mm256_loadu_si256((const __m256i*)pTarget), _mm256_loadu_si256((const __m256i*)pSource));
				_mm256_storeu_si256((__m256i*)pTarget, tgt);
			}
			for (size_t n = nrVectors * 8; n < HistogramSize(); ++n)
				targetHisto[n] += sourceHisto[n];
		}
		else // Why do we get here?
		{
			for (size_t n = 0; n < targetHisto.size(); ++n) // Let's hope, the targetHisto is larger in size than the sourceHisto.
				targetHisto[n] += sourceHisto[n];
		}
	};

	mergeHisto(red, redHisto);
	mergeHisto(green, greenHisto.empty() ? redHisto : greenHisto);
	mergeHisto(blue, blueHisto.empty() ? redHisto : blueHisto);

	return AvxSupport::zeroUpper(0);
}

bool AvxHistogram::histogramSuccessful() const
{
	return allRunsSuccessful;
};
