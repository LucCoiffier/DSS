#include "StdAfx.h"
#include "avx_output.h"
#include "avx_support.h"
#include "avx_median.h"
#include <immintrin.h>

AvxOutputComposition::AvxOutputComposition(CMultiBitmap& mBitmap, CMemoryBitmap& outputbm) :
	inputBitmap{ mBitmap },
	outputBitmap{ outputbm },
	avxReady{ true }
{
	if (!AvxSupport::checkSimdAvailability())
		avxReady = false;
	// Homogenization not implemented with AVX
	if (inputBitmap.GetHomogenization())
		avxReady = false;
	// Output must be float values
	if (AvxSupport{outputBitmap}.bitmapHasCorrectType<float>() == false)
		avxReady = false;
}

template <class INPUTTYPE, class OUTPUTTYPE>
static bool AvxOutputComposition::bitmapColorOrGray(const CMultiBitmap& bitmap) noexcept
{
	return
		(dynamic_cast<const CColorMultiBitmapT<INPUTTYPE, OUTPUTTYPE>*>(&bitmap) != nullptr) ||
		(dynamic_cast<const CGrayMultiBitmapT<INPUTTYPE, OUTPUTTYPE>*>(&bitmap) != nullptr);
}

template <class T>
inline static float AvxOutputComposition::convertToFloat(const T value) noexcept
{
	if constexpr (std::is_integral<T>::value && sizeof(T) == 4) // 32 bit integral type
		return static_cast<float>(value >> 16);
	else
		return static_cast<float>(value);
}

int AvxOutputComposition::compose(const int line, std::vector<void*> const& lineAddresses)
{
	if (!avxReady)
		return 1;
	// If this is not equal, something went wrong and we cannot continue without risking access violations.
	if (lineAddresses.size() != inputBitmap.GetNrAddedBitmaps())
		return 1;
	// No line addresses?
	if (lineAddresses.empty())
		return 1;

	int rval = 2;
	switch (inputBitmap.GetProcessingMethod())
	{
		case MBP_MEDIAN: rval = processMedianKappaSigma<MedianOnly>(line, lineAddresses); break;
		case MBP_SIGMACLIP: rval = processMedianKappaSigma<KappaSigma>(line, lineAddresses); break;
		case MBP_AUTOADAPTIVE: rval = processAutoAdaptiveWeightedAverage(line, lineAddresses); break;
		case MBP_MEDIANSIGMACLIP: rval = processMedianKappaSigma<MedianKappaSigma>(line, lineAddresses); break;
		default: rval = 2; break;
	}

	return AvxSupport::zeroUpper(rval);
}

template <AvxOutputComposition::MethodSelection Method>
int AvxOutputComposition::processMedianKappaSigma(const int line, std::vector<void*> const& lineAddresses)
{
	if (doProcessMedianKappaSigma<WORD, Method>(line, lineAddresses) == 0)
		return 0;
	if (doProcessMedianKappaSigma<unsigned long, Method>(line, lineAddresses) == 0)
		return 0;
	if (doProcessMedianKappaSigma<float, Method>(line, lineAddresses) == 0)
		return 0;
	return 1;
}

template <class T, AvxOutputComposition::MethodSelection Method>
int AvxOutputComposition::doProcessMedianKappaSigma(const int line, std::vector<void*> const& lineAddresses)
{
	// CMultiBitmap - template<TType, TTypeOutput>: Input must be of type T, and output type must be float.
	if (bitmapColorOrGray<T, float>(inputBitmap) == false)
		return 1;

	const auto parameters = inputBitmap.GetProcessingParameters();

	const int width = outputBitmap.RealWidth();
	const int nrVectors = width / 16;
	const int nrLightframes = static_cast<int>(lineAddresses.size());

	const auto accumulateSquared = [](const __m256d accumulator, const __m128 colorValue) noexcept -> __m256d
	{
		const __m256d pd = _mm256_cvtps_pd(colorValue);
		return _mm256_fmadd_pd(pd, pd, accumulator);
	};
	const auto sigma = [](const __m256 sum, const __m256d sumSqLo, const __m256d sumSqHi, const __m256 N) -> __m256
	{
		// Sigma� = sumSquared / N - �� = 1/N * (sumSquared - sum� / N)
		const __m256d Nlo = _mm256_cvtps_pd(_mm256_extractf128_ps(N, 0));
		const __m256d Nhi = _mm256_cvtps_pd(_mm256_extractf128_ps(N, 1));
		const __m256d sumLo = _mm256_cvtps_pd(_mm256_extractf128_ps(sum, 0));
		const __m256d sumHi = _mm256_cvtps_pd(_mm256_extractf128_ps(sum, 1));
		const __m256d sigmaSqLoN = _mm256_sub_pd(sumSqLo, _mm256_div_pd(_mm256_mul_pd(sumLo, sumLo), Nlo));
		const __m256d sigmaSqHiN = _mm256_sub_pd(sumSqHi, _mm256_div_pd(_mm256_mul_pd(sumHi, sumHi), Nhi));
		const __m256 sigmaSqN = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm256_cvtpd_ps(sigmaSqLoN)), _mm256_cvtpd_ps(sigmaSqHiN), 1);
		return _mm256_sqrt_ps(_mm256_div_ps(sigmaSqN, N));
	};
	const __m256 kappa = _mm256_set1_ps(static_cast<float>(std::get<0>(parameters)));


	// ************* Median *************

	std::vector<T> medianData(nrLightframes * size_t{ 16 }); // 16 pixels x nLightframes
	int sizes[16]; // Nr of lightframes != 0 for the 16 pixels

	const auto initMedianData = [&lineAddresses, &medianData, &sizes, nrLightframes](const size_t offset, const int nPixels) -> void
	{
		memset(medianData.data(), 0, medianData.size() * sizeof(T));
		memset(sizes, 0, sizeof(sizes));

		std::for_each(lineAddresses.cbegin(), lineAddresses.cend(), [&medianData, &sizes, offset, nPixels, nrLightframes](const void* const p)
		{
			const T* const pT = static_cast<const T*>(p);
			for (int n = 0; n < nPixels; ++n) // nPixels is 1..16
			{
				constexpr T zero = T{ 0 };
				const T element = *(pT + offset + n);
				auto& N = sizes[n];
				if (element != zero) // Copy all lightframe values that are != 0.
				{
					static_assert(!std::is_integral<T>::value || std::is_unsigned<T>::value);
					if constexpr (std::is_integral<T>::value && sizeof(T) == 4) // 32 bit integral type
						medianData[n * nrLightframes + (N++)] = (element >> 16);
					else
						medianData[n * nrLightframes + (N++)] = element;
				}
			}
		});
	};

	const auto quickMedian = [&medianData, &sizes, nrLightframes](const size_t pixelIndex, const float lBound, const float uBound, const float currMedian) -> float
	{
		const auto N = sizes[pixelIndex];
		if (N == 0)
			return 0.0f;

		const T lowerBound = static_cast<T>(lBound);
		const T upperBound = static_cast<T>(uBound);
		const T currentMedian = static_cast<T>(currMedian);
		T* const pData = medianData.data() + pixelIndex * nrLightframes;

		if constexpr (std::is_same<T, WORD>::value)
		{
			for (int n = 0; n < N / 8; ++n)
			{
				const __m128i v = _mm_loadu_si128(((const __m128i*)pData) + n);
				const __m256i v32 = _mm256_cvtepu16_epi32(v);
				const __m256i outOfRange = _mm256_or_si256(_mm256_cmpgt_epi32(_mm256_set1_epi32(lowerBound), v32), _mm256_cmpgt_epi32(v32, _mm256_set1_epi32(upperBound)));
				const __m256i vCorrected = _mm256_blendv_epi8(v32, _mm256_set1_epi32(currentMedian), outOfRange);
				_mm_storeu_si128(((__m128i*)pData) + n, AvxSupport::cvtEpi32Epu16(vCorrected));
			}
			for (int n = (N / 8) * 8; n < N; ++n)
				if (pData[n] < lowerBound || pData[n] > upperBound)
					pData[n] = currentMedian;
		}
		else // uint32, float
		{
			for (int n = 0; n < N; ++n) // All lightframes != 0
				if (pData[n] < lowerBound || pData[n] > upperBound)
					pData[n] = currentMedian;
		}

		return static_cast<float>(qMedian(pData, N, N / 2));
	};

	const auto vectorMedian = [&quickMedian](__m256& loMedian, __m256& hiMedian, const __m256 loLoBound, const __m256 hiLoBound, const __m256 loHiBound, const __m256 hiHiBound) -> void
	{
		for (size_t n = 0; n < 8; ++n)
			loMedian.m256_f32[n] = quickMedian(n, loLoBound.m256_f32[n], loHiBound.m256_f32[n], loMedian.m256_f32[n]);
		for (size_t n = 0; n < 8; ++n)
			hiMedian.m256_f32[n] = quickMedian(n + 8, hiLoBound.m256_f32[n], hiHiBound.m256_f32[n], hiMedian.m256_f32[n]);
	};


	// ************* Updater for the data sequences *************

	const auto zeroVectorUpdater = [](__m256& lo8, __m256& hi8, __m256& loN, __m256& hiN, const __m256 loOutOfRangeMask, const __m256 hiOutOfRangeMask) -> void
	{
		lo8 = _mm256_andnot_ps(loOutOfRangeMask, lo8); // Set to zero where value is outside my +- kappa * sigma.
		hi8 = _mm256_andnot_ps(hiOutOfRangeMask, hi8);
		loN = _mm256_add_ps(loN, _mm256_and_ps(_mm256_set1_ps(1.0f), _mm256_cmp_ps(lo8, _mm256_setzero_ps(), 12))); // if (value != 0) ++N; (12 means: not equal)
		hiN = _mm256_add_ps(hiN, _mm256_and_ps(_mm256_set1_ps(1.0f), _mm256_cmp_ps(hi8, _mm256_setzero_ps(), 12)));
	};
	const auto zeroScalarUpdater = [](float& value, float& N, const bool outOfRange) -> void
	{
		if (value == 0.0f)
			return;
		if (outOfRange)
			value = 0.0f;
		else
			++N; // If value != 0 -> ++N
	};
/*	const auto medianVectorUpdater = [](__m256& lo8, __m256& hi8, __m256& loN, __m256& hiN, const __m256 loMedian, const __m256 hiMedian, const __m256 loOutOfRangeMask, const __m256 hiOutOfRangeMask) -> void
	{
		const __m256 m0 = _mm256_cmp_ps(lo8, _mm256_setzero_ps(), 12); // != 0
		const __m256 m1 = _mm256_cmp_ps(hi8, _mm256_setzero_ps(), 12);
		lo8 = _mm256_and_ps(_mm256_blendv_ps(lo8, loMedian, loOutOfRangeMask), m0); // Set to median where value is out of range (outside my +- kappa * sigma).
		hi8 = _mm256_and_ps(_mm256_blendv_ps(hi8, hiMedian, hiOutOfRangeMask), m1); // If value was zero -> reset it to zero again.
		loN = _mm256_add_ps(loN, _mm256_and_ps(_mm256_set1_ps(1.0f), m0)); // if (value != 0) ++N;
		hiN = _mm256_add_ps(hiN, _mm256_and_ps(_mm256_set1_ps(1.0f), m1));
	};
	const auto medianScalarUpdater = [](float& value, float& N, const float median, const bool outOfRange) -> void
	{
		if (value == 0.0f)
			return;
		if (outOfRange)
			value = median;
		++N;
	};
*/

	const auto initialUpperBound = []() -> float
	{
		if constexpr (std::is_integral<T>::value)
			return static_cast<float>(std::numeric_limits<WORD>::max()); // We use 65535 for int16 and int32.
		else if constexpr (std::is_same<T, float>::value)
			return std::numeric_limits<float>::max();
		else
			static_assert(false);
	};

	// ************* Loops *************

	const auto kappaSigmaLoop = [&](float* pOut, const int colorOffset) -> void
	{
		const size_t outputWidth = outputBitmap.Width();

		for (int counter = 0; counter < nrVectors; ++counter, pOut += 16)
		{
			__m256 lowerBound1{ _mm256_setzero_ps() };
			__m256 lowerBound2{ _mm256_setzero_ps() };
			__m256 upperBound1{ _mm256_set1_ps(initialUpperBound()) };
			__m256 upperBound2{ _mm256_set1_ps(initialUpperBound()) };
			__m256 my1{ _mm256_undefined_ps() };
			__m256 my2{ _mm256_undefined_ps() };
			__m256 loMedian = _mm256_undefined_ps();
			__m256 hiMedian = _mm256_undefined_ps();

			if constexpr (Method == MedianKappaSigma)
				initMedianData(counter * size_t{ 16 } + colorOffset, 16);

			for (int iteration = 0; iteration < std::get<1>(parameters); ++iteration)
			{
				__m256 sum1 = _mm256_setzero_ps();
				__m256 sum2 = _mm256_setzero_ps();
				__m256 N1 = _mm256_setzero_ps();
				__m256 N2 = _mm256_setzero_ps();
				__m256d sumSq1 = _mm256_setzero_pd();
				__m256d sumSq2 = _mm256_setzero_pd();
				__m256d sumSq3 = _mm256_setzero_pd();
				__m256d sumSq4 = _mm256_setzero_pd();

				if constexpr (Method == MedianKappaSigma)
				{
					vectorMedian(loMedian, hiMedian, lowerBound1, lowerBound2, upperBound1, upperBound2);
					N1 = _mm256_cvtepi32_ps(_mm256_loadu_si256((const __m256i*)&sizes[0]));
					N2 = _mm256_cvtepi32_ps(_mm256_loadu_si256((const __m256i*)&sizes[8]));
				}

				// Loop over the light frames
//				for (auto frameAddress : lineAddresses)
				for (int lightFrame = 0; lightFrame < nrLightframes; ++lightFrame)
				{
					__m256 lo8, hi8;
					if constexpr (Method == MedianKappaSigma)
					{
						const T* const pColor = medianData.data() + lightFrame;
						const auto [l, h] = AvxSupport::read16PackedSingleStride(pColor, nrLightframes);
						lo8 = l;
						hi8 = h;
					}
					if constexpr (Method == KappaSigma)
					{
						const T* const pColor = static_cast<T*>(lineAddresses[lightFrame]) + counter * 16ULL + colorOffset;
						const auto [l, h] = AvxSupport::read16PackedSingle(pColor);
						lo8 = l;
						hi8 = h;
						const __m256 outOfRange1 = _mm256_or_ps(_mm256_cmp_ps(lo8, lowerBound1, 17), _mm256_cmp_ps(lo8, upperBound1, 30)); // 17: _CMP_LT_OQ, 30: _CMP_GT_OQ (x < lo OR x > hi)
						const __m256 outOfRange2 = _mm256_or_ps(_mm256_cmp_ps(hi8, lowerBound2, 17), _mm256_cmp_ps(hi8, upperBound2, 30));
						zeroVectorUpdater(lo8, hi8, N1, N2, outOfRange1, outOfRange2);
					}

					sum1 = _mm256_add_ps(sum1, lo8);
					sum2 = _mm256_add_ps(sum2, hi8);
					sumSq1 = accumulateSquared(sumSq1, _mm256_castps256_ps128(lo8));
					sumSq2 = accumulateSquared(sumSq2, _mm256_extractf128_ps(lo8, 1));
					sumSq3 = accumulateSquared(sumSq3, _mm256_castps256_ps128(hi8));
					sumSq4 = accumulateSquared(sumSq4, _mm256_extractf128_ps(hi8, 1));
				}
				// Calc the new averages
				const __m256 noValuesMask1 = _mm256_cmp_ps(N1, _mm256_setzero_ps(), 0);
				const __m256 noValuesMask2 = _mm256_cmp_ps(N2, _mm256_setzero_ps(), 0);
				my1 = _mm256_blendv_ps(_mm256_div_ps(sum1, N1), _mm256_setzero_ps(), noValuesMask1); // Low 8 floats. Set 0 where N==0.
				my2 = _mm256_blendv_ps(_mm256_div_ps(sum2, N2), _mm256_setzero_ps(), noValuesMask2); // Hi 8 floats. Set 0 where N==0.
				// Update lower and upper bound with new � +- kappa * sigma
				const __m256 sigma1 = sigma(sum1, sumSq1, sumSq2, N1);
				const __m256 sigma2 = sigma(sum2, sumSq3, sumSq4, N2);
				upperBound1 = _mm256_blendv_ps(_mm256_fmadd_ps(sigma1, kappa, my1), _mm256_setzero_ps(), noValuesMask1); // Set 0 where N==0.
				upperBound2 = _mm256_blendv_ps(_mm256_fmadd_ps(sigma2, kappa, my2), _mm256_setzero_ps(), noValuesMask2);
				lowerBound1 = _mm256_blendv_ps(_mm256_fnmadd_ps(sigma1, kappa, my1), _mm256_set1_ps(1.0f), noValuesMask1); // Set 1 where N==0.
				lowerBound2 = _mm256_blendv_ps(_mm256_fnmadd_ps(sigma2, kappa, my2), _mm256_set1_ps(1.0f), noValuesMask2);
			}
			_mm256_storeu_ps(pOut     + static_cast<size_t>(line) * outputWidth, my1);
			_mm256_storeu_ps(pOut + 8 + static_cast<size_t>(line) * outputWidth, my2);
		}
		// Rest of line
		for (int n = nrVectors * 16; n < width; ++n, ++pOut)
		{
			float lowerBound{ 1.0f };
			float upperBound{ static_cast<float>(std::numeric_limits<T>::max()) };
			float my{ 0.0f };
			float median{ 0.0f };

			if constexpr (Method == MedianKappaSigma)
				initMedianData(n + colorOffset, 1); // 1 = only 1 pixel.

			for (int iteration = 0; iteration < std::get<1>(parameters); ++iteration)
			{
				float sum{ 0.0f };
				float N{ 0.0f };
				float sumSq{ 0.0f };

				if constexpr (Method == MedianKappaSigma)
				{
					median = quickMedian(0, lowerBound, upperBound, median);
					N = static_cast<float>(sizes[0]);
				}

				for (int lightFrame = 0; lightFrame < nrLightframes; ++lightFrame)
				{
					float colorValue;
					if constexpr (Method == MedianKappaSigma)
					{
						const T* const pColor = medianData.data() + lightFrame;
						colorValue = *pColor;
					}
					if constexpr (Method == KappaSigma)
					{
						const T* const pColor = static_cast<T*>(lineAddresses[lightFrame]) + n + colorOffset;
						colorValue = convertToFloat(*pColor);
						zeroScalarUpdater(colorValue, N, colorValue < lowerBound || colorValue > upperBound);
					}

					sum += colorValue;
					sumSq += colorValue * colorValue;
				}
				my = (N == 0.0f ? 0.0f : (sum / N));
				if (N == 0.0f || N == static_cast<float>(nrLightframes))
					break;
				const float sigma = sqrtf(sumSq / N - my * my);
				const float sigmakappa = sigma * static_cast<float>(std::get<0>(parameters));
				lowerBound = my - sigmakappa;
				upperBound = my + sigmakappa;
			}
			*(pOut + static_cast<size_t>(line) * outputWidth) = my;
		}
	};

	const auto medianLoop = [&](float* pOut, const int colorOffset) -> void
	{
		const size_t outputWidth = outputBitmap.Width();
		const float uBound = initialUpperBound();

		for (int counter = 0; counter < nrVectors; ++counter, pOut += 16)
		{
			__m256 loMedian = _mm256_setzero_ps();
			__m256 hiMedian = _mm256_setzero_ps();

			initMedianData(counter * size_t{ 16 } + colorOffset, 16);
			vectorMedian(loMedian, hiMedian, _mm256_set1_ps(0.0f), _mm256_set1_ps(0.0f), _mm256_set1_ps(uBound), _mm256_set1_ps(uBound));
			_mm256_storeu_ps(pOut + static_cast<size_t>(line) * outputWidth, loMedian);
			_mm256_storeu_ps(pOut + 8 + static_cast<size_t>(line) * outputWidth, hiMedian);
		}
		// Rest of line
		for (int n = nrVectors * 16; n < width; ++n, ++pOut)
		{
			initMedianData(n + colorOffset, 1);
			const float median = quickMedian(0, 0.0f, uBound, 0.0f);
			*(pOut + static_cast<size_t>(line) * outputWidth) = median;
		}
	};

	const auto methodSelectorLoop = [&](float* pOut, const int colorOffset) -> void
	{
		if constexpr (Method == MedianOnly)
			medianLoop(pOut, colorOffset);
		else
			kappaSigmaLoop(pOut, colorOffset);
	};

	if (auto pOutputBitmap{ dynamic_cast<C96BitFloatColorBitmap*>(&outputBitmap) })
	{
		methodSelectorLoop(&*pOutputBitmap->m_Red.m_vPixels.begin(), 0);
		methodSelectorLoop(&*pOutputBitmap->m_Green.m_vPixels.begin(), width);
		methodSelectorLoop(&*pOutputBitmap->m_Blue.m_vPixels.begin(), width * 2);
		return 0;
	}
	if (auto pOutputBitmap{ dynamic_cast<C32BitFloatGrayBitmap*>(&outputBitmap) })
	{
		methodSelectorLoop(&*pOutputBitmap->m_vPixels.begin(), 0);
		return 0;
	}

	// Neither gray (1 float) nor color (3 floats).
	return 1;
}


int AvxOutputComposition::processAutoAdaptiveWeightedAverage(const int line, std::vector<void*> const& lineAddresses)
{
	if (doProcessAutoAdaptiveWeightedAverage<WORD>(line, lineAddresses) == 0)
		return 0;
	if (doProcessAutoAdaptiveWeightedAverage<unsigned long>(line, lineAddresses) == 0)
		return 0;
	if (doProcessAutoAdaptiveWeightedAverage<float>(line, lineAddresses) == 0)
		return 0;
	return 1;
}

template <class T>
int AvxOutputComposition::doProcessAutoAdaptiveWeightedAverage(const int line, std::vector<void*> const& lineAddresses)
{
	// CMultiBitmap - template<TType, TTypeOutput>: Input must be of type T, and output type must be float.
	if (bitmapColorOrGray<T, float>(inputBitmap) == false)
		return 1;

	const int nIterations = std::get<1>(inputBitmap.GetProcessingParameters());
	const int width = outputBitmap.RealWidth();
	const int nrVectors = width / 16;
	const __m256 N = _mm256_set1_ps(static_cast<float>(lineAddresses.size()));
	const size_t outputWidth = outputBitmap.Width();

	const auto autoAdaptLoop = [&](float* pOut, const int colorOffset) -> void
	{
		// Loop over the pixels of the row, process 16 at a time.
		for (int counter = 0; counter < nrVectors; ++counter, pOut += 16)
		{
			__m256 my1 = _mm256_setzero_ps();
			__m256 my2 = _mm256_setzero_ps();

			// Calculate initial (unweighted) mean.
			for (auto frameAddress : lineAddresses)
			{
				const T *const pColor = static_cast<T*>(frameAddress) + counter * 16ULL + colorOffset;
				const auto [lo8, hi8] = AvxSupport::read16PackedSingle(pColor);
				my1 = _mm256_add_ps(my1, lo8);
				my2 = _mm256_add_ps(my2, hi8);
			}
			my1 = _mm256_div_ps(my1, N); // N != 0 guaranteed
			my2 = _mm256_div_ps(my2, N);

			for (int iteration = 0; iteration < nIterations; ++iteration)
			{
				__m256 S1 = _mm256_setzero_ps();
				__m256 S2 = _mm256_setzero_ps();

				// Calculate sigma� related to � of last iteration.
				for (auto frameAddress : lineAddresses)
				{
					const T *const pColor = static_cast<T*>(frameAddress) + counter * 16ULL + colorOffset;
					const auto [lo8, hi8] = AvxSupport::read16PackedSingle(pColor);
					const __m256 d1 = _mm256_sub_ps(lo8, my1);
					const __m256 d2 = _mm256_sub_ps(hi8, my2);
					S1 = _mm256_fmadd_ps(d1, d1, S1); // Sum of (x-�)�
					S2 = _mm256_fmadd_ps(d2, d2, S2);
				}
				const __m256 sigmaSq1 = _mm256_div_ps(S1, N); // sigma� = sum(x-�)� / N
				const __m256 sigmaSq2 = _mm256_div_ps(S2, N);

				// Calculate new � using current sigma�.
				__m256 W1 = _mm256_setzero_ps();
				__m256 W2 = _mm256_setzero_ps();
				S1 = _mm256_setzero_ps();
				S2 = _mm256_setzero_ps();
				for (auto frameAddress : lineAddresses)
				{
					const T *const pColor = static_cast<T*>(frameAddress) + counter * 16ULL + colorOffset;
					const auto [lo8, hi8] = AvxSupport::read16PackedSingle(pColor);
					const __m256 d1 = _mm256_sub_ps(lo8, my1); // x-�
					const __m256 d2 = _mm256_sub_ps(hi8, my2);
					const __m256 denominator1 = _mm256_fmadd_ps(d1, d1, sigmaSq1); // sigma� + (x-�)�
					const __m256 denominator2 = _mm256_fmadd_ps(d2, d2, sigmaSq2);
					const __m256 weight1 = _mm256_blendv_ps(_mm256_div_ps(sigmaSq1, denominator1), _mm256_set1_ps(1.0f), _mm256_cmp_ps(denominator1, _mm256_setzero_ps(), 0)); // sigma� / (sigma� + (x-�)�) = 1 / (1 + (x-�)�/sigma�)
					const __m256 weight2 = _mm256_blendv_ps(_mm256_div_ps(sigmaSq2, denominator2), _mm256_set1_ps(1.0f), _mm256_cmp_ps(denominator2, _mm256_setzero_ps(), 0)); // Set weight to 1 when sigma==0.
					W1 = _mm256_add_ps(W1, weight1); // W = sum(weights)
					W2 = _mm256_add_ps(W2, weight2);
					S1 = _mm256_fmadd_ps(lo8, weight1, S1); // S = sum(x * weight)
					S2 = _mm256_fmadd_ps(hi8, weight2, S2);
				}

				my1 = _mm256_div_ps(S1, W1); // W == 0 (sum of weights) cannot happen.
				my2 = _mm256_div_ps(S2, W2);
			}
			_mm256_storeu_ps(pOut     + static_cast<size_t>(line) * outputWidth, my1);
			_mm256_storeu_ps(pOut + 8 + static_cast<size_t>(line) * outputWidth, my2);
		}

		// Rest of line
		const float N = static_cast<float>(lineAddresses.size());
		for (int n = nrVectors * 16; n < width; ++n, ++pOut)
		{
			float my{ 0.0f };
			// Calculate initial (unweighted) mean.
			for (auto frameAddress : lineAddresses)
			{
				const T *const pColor = static_cast<T*>(frameAddress) + n + colorOffset;
				my += convertToFloat(*pColor);
			}
			my /= N;

			for (int iteration = 0; iteration < nIterations; ++iteration)
			{
				float S{ 0.0f };

				// Calculate sigma� related to � of last iteration.
				for (auto frameAddress : lineAddresses)
				{
					const T *const pColor = static_cast<T*>(frameAddress) + n + colorOffset;
					const float d = convertToFloat(*pColor) - my;
					S += (d * d);
				}
				const float sigmaSq = S / N;

				// Calculate new � using current sigma�.
				float W{ 0.0f };
				S = 0.0f;
				for (auto frameAddress : lineAddresses)
				{
					const T *const pColor = static_cast<T*>(frameAddress) + n + colorOffset;
					const float color = convertToFloat(*pColor);
					const float d = color - my;
					const float denominator = sigmaSq + d * d;
					const float w = denominator == 0.0f ? 1.0f : (sigmaSq / denominator);
					W += w;
					S += color * w;
				}
				my = S / W; // W cannot be zero.
			}

			*(pOut + static_cast<size_t>(line) * outputWidth) = my;
		}
	};

	if (auto pOutputBitmap{ dynamic_cast<C96BitFloatColorBitmap*>(&outputBitmap) })
	{
		autoAdaptLoop(&*pOutputBitmap->m_Red.m_vPixels.begin(), 0);
		autoAdaptLoop(&*pOutputBitmap->m_Green.m_vPixels.begin(), width);
		autoAdaptLoop(&*pOutputBitmap->m_Blue.m_vPixels.begin(), width * 2);
		return 0;
	}
	if (auto pOutputBitmap{ dynamic_cast<C32BitFloatGrayBitmap*>(&outputBitmap) })
	{
		autoAdaptLoop(&*pOutputBitmap->m_vPixels.begin(), 0);
		return 0;
	}

	// Neither gray (1 float) nor color (3 floats).
	return 1;
}
