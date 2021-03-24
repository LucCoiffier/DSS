#include "StdAfx.h"
#include "avx_cfa.h"
#include "avx_support.h"
#include <immintrin.h>

AvxCfaProcessing::AvxCfaProcessing(const size_t lineStart, const size_t lineEnd, CMemoryBitmap& inputbm) :
	redPixels{},
	greenPixels{},
	bluePixels{},
	inputBitmap{ inputbm },
	vectorsPerLine{ 0 }
{
	init(lineStart, lineEnd);
}

void AvxCfaProcessing::init(const size_t lineStart, const size_t lineEnd) // You should be sure that lineEnd >= lineStart!
{
	const size_t height = lineEnd - lineStart;
	vectorsPerLine = AvxSupport::numberOfAvxVectors<WORD, VectorElementType>(inputBitmap.Width());
	const size_t nrVectors = vectorsPerLine * height;
	if (nrVectors != 0 && AvxSupport{ inputBitmap }.isMonochromeCfaBitmapOfType<WORD>())
	{
		redPixels.resize(nrVectors);
		greenPixels.resize(nrVectors);
		bluePixels.resize(nrVectors);
	}
}

int AvxCfaProcessing::interpolate(const size_t lineStart, const size_t lineEnd, const long pixelSizeMultiplier)
{
	if (!AvxSupport::checkSimdAvailability())
		return 1;
	if (pixelSizeMultiplier != 1)
		return 1;
	if (!AvxSupport{ inputBitmap }.isMonochromeCfaBitmapOfType<WORD>())
		return 1;

	return interpolateGrayCFA2Color(lineStart, lineEnd);
}

int AvxCfaProcessing::interpolateGrayCFA2Color(const size_t lineStart, const size_t lineEnd)
{
	typedef std::tuple<__m256i, __m256i, __m256i> ColorVector16; // <R, G, B>: get<0>() returns Red, ...

	if (auto* const p{ dynamic_cast<CGrayBitmapT<WORD>*>(&inputBitmap) })
	{
		if (!p->IsCFA())
			return 1;
	}
	else
		return 1;

	const size_t width = inputBitmap.Width();
	const size_t height = lineEnd - lineStart;
	const size_t inputHeight = inputBitmap.Height();
	// AVX makes no sense for super-small arrays.
	if (width < 64 || inputHeight < 8)
		return 1;

	const auto avg2Epu16 = [](const __m256i a, const __m256i b) -> __m256i
	{
		return _mm256_avg_epu16(a, b); // 17 bit temp = 1 + a(i) + b(i); return (temp >> 1);
	};
	const auto avg4Epu16 = [&avg2Epu16](const __m256i a, const __m256i b, const __m256i c, const __m256i d) -> __m256i
	{
		return avg2Epu16(avg2Epu16(a, b), avg2Epu16(c, d));
	};
	const auto shiftOnePixelRightEpu16 = [](const __m256i currentVector, const __m256i previousVector) -> __m256i
	{
		const __m256i right1 = _mm256_slli_si256(currentVector, 2);
		const __m256i b = _mm256_permute2x128_si256(currentVector, previousVector, 0x03);
		const __m256i left7 = _mm256_srli_si256(b, 14);
		return _mm256_or_si256(right1, left7);
	};
	const auto shiftOnePixelLeftEpu16 = [](const __m256i currentVector, const __m256i nextVector) -> __m256i
	{
		const __m256i left1 = _mm256_srli_si256(currentVector, 2);
		const __m256i b = _mm256_permute2x128_si256(currentVector, nextVector, 0x21);
		const __m256i right7 = _mm256_slli_si256(b, 14);
		return _mm256_or_si256(left1, right7);
	};
	const auto load16Pixels = [](const WORD* const p) -> __m256i
	{
		return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
	};

	const size_t nrVectors = width / 16;
	const size_t remainingPixels = width % 16;
	const AvxSupport avxSupport{ inputBitmap };

	// 3 x 16 pixel vectors - current line
	__m256i thisLineCurrent = _mm256_undefined_si256();
	__m256i thisLineNext = _mm256_undefined_si256(); // Next 16 pixels
	__m256i thisLinePrev = _mm256_undefined_si256(); // Previous 16 pixels
	// 3 x 16 pixel vectors - next line
	__m256i nextLineCurrent = _mm256_undefined_si256();
	__m256i nextLineNext = _mm256_undefined_si256();
	__m256i nextLinePrev = _mm256_undefined_si256();
	// 3 x 16 pixel vectors - previous line
	__m256i prevLineCurrent = _mm256_undefined_si256();
	__m256i prevLineNext = _mm256_undefined_si256();
	__m256i prevLinePrev = _mm256_undefined_si256();

	// R, G, R, G, ...: each is epu16
	const auto interpolateRGlineEpu16 = [&]() noexcept -> ColorVector16
	{
		const __m256i onePixelRight = shiftOnePixelRightEpu16(thisLineCurrent, thisLinePrev);
		const __m256i onePixelLeft = shiftOnePixelLeftEpu16(thisLineCurrent, thisLineNext);
		const __m256i interpolated = avg2Epu16(onePixelRight, onePixelLeft);
		// Red color
		const __m256i red = _mm256_blend_epi16(thisLineCurrent, interpolated, 0xaa);
		// Green color
		const __m256i green = _mm256_blend_epi16(thisLineCurrent, interpolated, 0x55);
		// Blue color
		const __m256i gColumn = avg2Epu16(prevLineCurrent, nextLineCurrent);
		const __m256i prevOneRight = shiftOnePixelRightEpu16(prevLineCurrent, prevLinePrev);
		const __m256i prevOneLeft = shiftOnePixelLeftEpu16(prevLineCurrent, prevLineNext);
		const __m256i nextOneRight = shiftOnePixelRightEpu16(nextLineCurrent, nextLinePrev);
		const __m256i nextOneLeft = shiftOnePixelLeftEpu16(nextLineCurrent, nextLineNext);
		const __m256i rColumn = avg4Epu16(prevOneRight, prevOneLeft, nextOneRight, nextOneLeft);
		const __m256i blue = _mm256_blend_epi16(rColumn, gColumn, 0xaa);
		return { red, green, blue };
	};
	// G, B, G, B, ...: each is epu16
	const auto interpolateGBlineEpu16 = [&]() noexcept -> ColorVector16
	{
		// Red color
		const __m256i gColumn = avg2Epu16(prevLineCurrent, nextLineCurrent);
		const __m256i prevOneRight = shiftOnePixelRightEpu16(prevLineCurrent, prevLinePrev);
		const __m256i prevOneLeft = shiftOnePixelLeftEpu16(prevLineCurrent, prevLineNext);
		const __m256i nextOneRight = shiftOnePixelRightEpu16(nextLineCurrent, nextLinePrev);
		const __m256i nextOneLeft = shiftOnePixelLeftEpu16(nextLineCurrent, nextLineNext);
		const __m256i bColumn = avg4Epu16(prevOneRight, prevOneLeft, nextOneRight, nextOneLeft);
		const __m256i red = _mm256_blend_epi16(gColumn, bColumn, 0xaa);
		// Green color
		const __m256i onePixelRight = shiftOnePixelRightEpu16(thisLineCurrent, thisLinePrev);
		const __m256i onePixelLeft = shiftOnePixelLeftEpu16(thisLineCurrent, thisLineNext);
		const __m256i gInterpolated = avg4Epu16(prevLineCurrent, nextLineCurrent, onePixelRight, onePixelLeft);
		const __m256i green = _mm256_blend_epi16(thisLineCurrent, gInterpolated, 0xaa);
		// Blue color
		const __m256i bInterpolated = avg2Epu16(onePixelRight, onePixelLeft);
		const __m256i blue = _mm256_blend_epi16(thisLineCurrent, bInterpolated, 0x55);
		return { red, green, blue };
	};
	const auto advanceVectorsEpu16 = [&](const WORD* const pCFA, const WORD* const pCFAnext, const WORD* const pCFAprev, const bool loadCurrent, const bool loadNext, const bool loadPrev) -> void
	{
		thisLinePrev = thisLineCurrent;
		thisLineCurrent = thisLineNext;
		if (loadCurrent)
			thisLineNext = load16Pixels(pCFA + 16);

		nextLinePrev = nextLineCurrent;
		nextLineCurrent = nextLineNext;
		if (loadNext)
			nextLineNext = load16Pixels(pCFAnext + 16);

		prevLinePrev = prevLineCurrent;
		prevLineCurrent = prevLineNext;
		if (loadPrev)
			prevLineNext = load16Pixels(pCFAprev + 16);
	};
	const auto storeRGBvectorsEpu16 = [](const __m256i red, const __m256i green, const __m256i blue, WORD* const pRed, WORD* const pGreen, WORD* const pBlue) -> void
	{
		_mm256_storeu_si256((__m256i*)pRed, red);
		_mm256_storeu_si256((__m256i*)pGreen, green);
		_mm256_storeu_si256((__m256i*)pBlue, blue);
	};

	const WORD* pCFA{ &avxSupport.grayPixels<WORD>().at(lineStart * width) };
	const WORD* pCFAnext{ lineStart == (inputHeight - 1) ? (pCFA - width) : (pCFA + width) };
	const WORD* pCFAprev{ lineStart == 0 ? pCFAnext : (pCFA - width) };
	WORD* pRed{ redCfaLine(0) };
	WORD* pGreen{ greenCfaLine(0) };
	WORD* pBlue{ blueCfaLine(0) };

	const auto advanceCFApointers = [&]() -> void
	{
		std::advance(pCFA, 16);
		std::advance(pCFAnext, 16);
		std::advance(pCFAprev, 16);
		std::advance(pRed, 16);
		std::advance(pGreen, 16);
		std::advance(pBlue, 16);
	};
	const auto loadLastVector = [remainingPixels](__m256i& vector, const WORD* const pPixels) -> void
	{
		if (remainingPixels == 0)
			return;
		__m256i tempVec = _mm256_setzero_si256();
		for (int n = 0; n < remainingPixels; ++n)
			tempVec.m256i_u16[n] = *(pPixels + 16 + n);
		vector = tempVec;
	};
	const auto lastVectorsOfLine = [&](const size_t lineNdx) -> void
	{
		switch (inputHeight - lineNdx) {
		case 1: advanceVectorsEpu16(pCFA, pCFAnext, pCFAprev, false, true, true); loadLastVector(thisLineNext, pCFA); break; // last line: Load next vector of current line only partially.
		case 2: advanceVectorsEpu16(pCFA, pCFAnext, pCFAprev, true, false, true); loadLastVector(nextLineNext, pCFAnext); break; // second last line: Load next vector of next line only partially.
		default: advanceVectorsEpu16(pCFA, pCFAnext, pCFAprev, true, true, true);
		}
		if (remainingPixels == 0) // No pixels left to process at the end of the line
		{
			thisLineNext = _mm256_setzero_si256();
			nextLineNext = _mm256_setzero_si256();
			prevLineNext = _mm256_setzero_si256();
		}
	};
	const auto storeRemainingPixels = [&pRed, &pGreen, &pBlue, remainingPixels](const __m256i red, const __m256i green, const __m256i blue) -> void
	{
		for (int n = 0; n < remainingPixels; ++n, ++pRed, ++pGreen, ++pBlue)
		{
			*pRed = red.m256i_u16[n];
			*pGreen = green.m256i_u16[n];
			*pBlue = blue.m256i_u16[n];
		}
	};

	for (size_t row = 0, lineNdx = lineStart; row < height; ++row, ++lineNdx)
	{
		thisLineCurrent = load16Pixels(pCFA);
		thisLineNext = load16Pixels(pCFA + 16);
		thisLinePrev = _mm256_setzero_si256();

		nextLineCurrent = load16Pixels(pCFAnext);
		nextLineNext = load16Pixels(pCFAnext + 16);
		nextLinePrev = _mm256_setzero_si256();

		prevLineCurrent = load16Pixels(pCFAprev);
		prevLineNext = load16Pixels(pCFAprev + 16);
		prevLinePrev = _mm256_setzero_si256();

		pRed = redCfaLine(row);
		pGreen = greenCfaLine(row);
		pBlue = blueCfaLine(row);

		if ((lineNdx & 0x01) == 0)
		{
			const auto [r, g, b] = interpolateRGlineEpu16();
			storeRGBvectorsEpu16(r, g, b, pRed, pGreen, pBlue);
			advanceCFApointers();

			for (int counter = 1; counter < nrVectors - 1; ++counter)
			{
				advanceVectorsEpu16(pCFA, pCFAnext, pCFAprev, true, true, true);
				const auto [r, g, b] = interpolateRGlineEpu16();
				storeRGBvectorsEpu16(r, g, b, pRed, pGreen, pBlue);
				advanceCFApointers();
			}
			// Last vector
			lastVectorsOfLine(lineNdx);
			const auto [red, green, blue] = interpolateRGlineEpu16();
			storeRGBvectorsEpu16(red, green, blue, pRed, pGreen, pBlue);
			advanceCFApointers();
			// Remaining pixels of line (R, G, ...)
			if (remainingPixels != 0)
			{
				advanceVectorsEpu16(pCFA, pCFAnext, pCFAprev, false, false, false);
				const auto [r, g, b] = interpolateRGlineEpu16();
				storeRemainingPixels(r, g, b);
				std::advance(pCFA, remainingPixels);
			}
		}
		else
		{
			const auto [r, g, b] = interpolateGBlineEpu16();
			storeRGBvectorsEpu16(r, g, b, pRed, pGreen, pBlue);
			advanceCFApointers();

			for (int counter = 1; counter < nrVectors - 1; ++counter)
			{
				advanceVectorsEpu16(pCFA, pCFAnext, pCFAprev, true, true, true);
				const auto [r, g, b] = interpolateGBlineEpu16();
				storeRGBvectorsEpu16(r, g, b, pRed, pGreen, pBlue);
				advanceCFApointers();
			}
			// Last vector
			lastVectorsOfLine(lineNdx);
			const auto [red, green, blue] = interpolateGBlineEpu16();
			storeRGBvectorsEpu16(red, green, blue, pRed, pGreen, pBlue);
			advanceCFApointers();
			// Remaining pixels of line (G, B, ...)
			if (remainingPixels != 0)
			{
				advanceVectorsEpu16(pCFA, pCFAnext, pCFAprev, false, false, false);
				const auto [r, g, b] = interpolateGBlineEpu16();
				storeRemainingPixels(r, g, b);
				std::advance(pCFA, remainingPixels);
			}
		}

		pCFAprev = pCFA - width;
		if (lineNdx == inputHeight - 2) // Next round will be the last line of bitmap -> next_line = previous_line (then the interpolation is correct)
			pCFAnext = pCFAprev;
		else
			pCFAnext = pCFA + width;
	}

	return 0;
}
