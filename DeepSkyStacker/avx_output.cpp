#include "StdAfx.h"
#include "avx_output.h"
#include "avx.h"

#if defined(AVX_INTRINSICS) && defined(_M_X64)

#include <immintrin.h>

AvxOutputComposition::AvxOutputComposition(CMultiBitmap& mBitmap, CMemoryBitmap& outputbm) :
	inputBitmap{ mBitmap },
	outputBitmap{ outputbm },
	avxReady{ true }
{
	if (!AvxSupport::checkCpuFeatures())
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

	switch (inputBitmap.GetProcessingMethod())
	{
		case MBP_SIGMACLIP: return processKappaSigma(line, lineAddresses);
		case MBP_AUTOADAPTIVE: return processAutoAdaptiveWeightedAverage(line, lineAddresses);
		default: return 2;
	}

	return 2;
}

int AvxOutputComposition::processKappaSigma(const int line, std::vector<void*> const& lineAddresses)
{
	if (doProcessKappaSigma<WORD>(line, lineAddresses) == 0)
		return 0;
	if (doProcessKappaSigma<std::uint32_t>(line, lineAddresses) == 0)
		return 0;
	if (doProcessKappaSigma<float>(line, lineAddresses) == 0)
		return 0;
	return 1;
}

template <class T>
int AvxOutputComposition::doProcessKappaSigma(const int line, std::vector<void*> const& lineAddresses)
{
	// CMultiBitmap - template<TType, TTypeOutput>: Input must be of type T, and output type must be float.
	if (bitmapColorOrGray<T, float>(inputBitmap) == false)
		return 1;

	const auto parameters = inputBitmap.GetProcessingParameters();

	const int width = outputBitmap.RealWidth();
	const int nrVectors = width / 16;

/*	const auto ps2epu16 = [](const __m256 x1, const __m256 x2) noexcept -> __m256i
	{
		return _mm256_permute4x64_epi64(_mm256_packus_epi32(_mm256_cvtps_epi32(x1), _mm256_cvtps_epi32(x2)), 0xd8);
	};
*/
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
/*	const auto sigmakappa = [&parameters](const __m256 sigma) noexcept -> __m256
	{
		return _mm256_mul_ps(sigma, _mm256_set1_ps(static_cast<float>(std::get<0>(parameters))));
	};
*/
	const __m256 kappa = _mm256_set1_ps(static_cast<float>(std::get<0>(parameters)));

	const auto kappaSigmaLoop = [&](float* pOut, const int colorOffset) -> void
	{
		const size_t outputWidth = outputBitmap.Width();

		for (int counter = 0; counter < nrVectors; ++counter, pOut += 16)
		{
//			__m256i lowerBound = _mm256_set1_epi16(WORD{1}); // Values of zero are ignored.
//			__m256i upperBound = _mm256_set1_epi16(std::numeric_limits<WORD>::max());
			__m256 lowerBound1{ _mm256_setzero_ps() };
			__m256 lowerBound2{ _mm256_setzero_ps() };
			__m256 upperBound1{ _mm256_set1_ps(static_cast<float>(std::numeric_limits<T>::max())) };
			__m256 upperBound2{ _mm256_set1_ps(static_cast<float>(std::numeric_limits<T>::max())) };
			__m256 my1{ _mm256_undefined_ps() };
			__m256 my2{ _mm256_undefined_ps() };

			for (int iteration = 0; iteration < std::get<1>(parameters); ++iteration)
			{
				__m256 sum1 = _mm256_setzero_ps();
				__m256 sum2 = _mm256_setzero_ps();
//				__m256i N = _mm256_setzero_si256();
				__m256 N1{ _mm256_setzero_ps() };
				__m256 N2{ _mm256_setzero_ps() };
				__m256d sumSq1 = _mm256_setzero_pd();
				__m256d sumSq2 = _mm256_setzero_pd();
				__m256d sumSq3 = _mm256_setzero_pd();
				__m256d sumSq4 = _mm256_setzero_pd();
				// Loop over the light frames
				for (auto frameAddress : lineAddresses)
				{
					const T *const pColor = static_cast<T*>(frameAddress) + counter * 16ULL + colorOffset;
					auto [lo8, hi8] = AvxSupport::read16PackedSingle(pColor);
					const __m256 outOfRange1 = _mm256_or_ps(_mm256_cmp_ps(lo8, lowerBound1, 17), _mm256_cmp_ps(lo8, upperBound1, 30)); // 17: _CMP_LT_OQ, 30: _CMP_GT_OQ (x < lo OR x > hi)
					const __m256 outOfRange2 = _mm256_or_ps(_mm256_cmp_ps(hi8, lowerBound2, 17), _mm256_cmp_ps(hi8, upperBound2, 30));
					lo8 = _mm256_andnot_ps(outOfRange1, lo8);
					hi8 = _mm256_andnot_ps(outOfRange2, hi8);

/*					const WORD* pColor = static_cast<WORD*>(frameAddress) + counter * 16ULL + colorOffset;
					const __m256i colorValue = _mm256_loadu_si256((const __m256i*)pColor);
					const __m256i outOfRangeMask = _mm256_or_si256(AvxSupport::cmpGtEpu16(lowerBound, colorValue), AvxSupport::cmpGtEpu16(colorValue, upperBound)); // 16 x 16 bit mask. One where value out of range.
					const __m256i effectiveValue = _mm256_andnot_si256(outOfRangeMask, colorValue); // Zero if outside (� +- kappa*sigma).
					const __m256 lo8 = AvxSupport::wordToPackedFloat(_mm256_extracti128_si256(effectiveValue, 0));
					const __m256 hi8 = AvxSupport::wordToPackedFloat(_mm256_extracti128_si256(effectiveValue, 1));
*/
					sum1 = _mm256_add_ps(sum1, lo8);
					sum2 = _mm256_add_ps(sum2, hi8);
					sumSq1 = accumulateSquared(sumSq1, _mm256_extractf128_ps(lo8, 0));
					sumSq2 = accumulateSquared(sumSq2, _mm256_extractf128_ps(lo8, 1));
					sumSq3 = accumulateSquared(sumSq3, _mm256_extractf128_ps(hi8, 0));
					sumSq4 = accumulateSquared(sumSq4, _mm256_extractf128_ps(hi8, 1));
					N1 = _mm256_add_ps(N1, _mm256_blendv_ps(_mm256_set1_ps(1.0f), _mm256_setzero_ps(), outOfRange1));
					N2 = _mm256_add_ps(N2, _mm256_blendv_ps(_mm256_set1_ps(1.0f), _mm256_setzero_ps(), outOfRange2));
//					N = _mm256_adds_epu16(N, _mm256_blendv_epi8(_mm256_set1_epi16(short{1}), _mm256_setzero_si256(), outOfRangeMask));
				}
				// Calc the new averages
//				const __m256 N1 = AvxSupport::wordToPackedFloat(_mm256_extracti128_si256(N, 0));
//				const __m256 N2 = AvxSupport::wordToPackedFloat(_mm256_extracti128_si256(N, 1));
				const __m256 noValuesMask1 = _mm256_cmp_ps(N1, _mm256_setzero_ps(), 0);
				const __m256 noValuesMask2 = _mm256_cmp_ps(N2, _mm256_setzero_ps(), 0);
				my1 = _mm256_blendv_ps(_mm256_div_ps(sum1, N1), _mm256_setzero_ps(), noValuesMask1); // Low 8 floats. Set 0 where N==0.
				my2 = _mm256_blendv_ps(_mm256_div_ps(sum2, N2), _mm256_setzero_ps(), noValuesMask2); // Hi 8 floats. Set 0 where N==0.
				// Update lower and upper bound with new � +- kappa * sigma
				const __m256 sigma1 = sigma(sum1, sumSq1, sumSq2, N1);
				const __m256 sigma2 = sigma(sum2, sumSq3, sumSq4, N2);
//				const __m256 sigmakappa1 = sigmakappa(sigma1);
//				const __m256 sigmakappa2 = sigmakappa(sigma2);
//				const __m256 upper1 = _mm256_add_ps(my1, sigmakappa1); // � + sigma * kappa
//				const __m256 upper2 = _mm256_add_ps(my2, sigmakappa2);
//				const __m256 lower1 = _mm256_sub_ps(my1, sigmakappa1); // � - sigma * kappa
//				const __m256 lower2 = _mm256_sub_ps(my2, sigmakappa2);
//				const __m256i noValuesMask = _mm256_cmpeq_epi16(N, _mm256_setzero_si256()); // 16 x 16 bit mask. One where N==0.
//				lowerBound = _mm256_blendv_epi8(ps2epu16(lower1, lower2), _mm256_set1_epi16(WORD{1}), noValuesMask); // Set 1 where N==0.
//				upperBound = _mm256_blendv_epi8(ps2epu16(upper1, upper2), _mm256_setzero_si256(), noValuesMask); // Set 0 where N==0.
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

			for (int iteration = 0; iteration < std::get<1>(parameters); ++iteration)
			{
				float sum{ 0.0f };
				float N{ 0.0f };
				float sumSq{ 0.0f };
				for (auto frameAddress : lineAddresses)
				{
					const T *const pColor = static_cast<T*>(frameAddress) + n + colorOffset;
					const float colorValue = static_cast<float>(*pColor);
					if (colorValue >= lowerBound && colorValue <= upperBound)
					{
						sum += colorValue;
						sumSq += colorValue * colorValue;
						++N;
					}
				}
				my = (N == 0.0f ? 0.0f : (sum / N));
				if (N == 0.0f || N == static_cast<float>(lineAddresses.size()))
					break;
				const float sigma = sqrtf(sumSq / N - my * my);
				const float sigmakappa = sigma * static_cast<float>(std::get<0>(parameters));
				lowerBound = my - sigmakappa;
				upperBound = my + sigmakappa;
			}
			*(pOut + static_cast<size_t>(line) * outputWidth) = my;
		}
	};

	if (auto pOutputBitmap{ dynamic_cast<C96BitFloatColorBitmap*>(&outputBitmap) })
	{
		kappaSigmaLoop(&*pOutputBitmap->m_Red.m_vPixels.begin(), 0);
		kappaSigmaLoop(&*pOutputBitmap->m_Green.m_vPixels.begin(), width);
		kappaSigmaLoop(&*pOutputBitmap->m_Blue.m_vPixels.begin(), width * 2);
		return 0;
	}
	if (auto pOutputBitmap{ dynamic_cast<C32BitFloatGrayBitmap*>(&outputBitmap) })
	{
		kappaSigmaLoop(&*pOutputBitmap->m_vPixels.begin(), 0);
		return 0;
	}

	// Neither gray (1 float) nor color (3 floats).
	return 1;
}


int AvxOutputComposition::processAutoAdaptiveWeightedAverage(const int line, std::vector<void*> const& lineAddresses)
{
	if (doProcessAutoAdaptiveWeightedAverage<WORD>(line, lineAddresses) == 0)
		return 0;
	if (doProcessAutoAdaptiveWeightedAverage<std::uint32_t>(line, lineAddresses) == 0)
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
/*				const __m256i colorValue = _mm256_loadu_si256((const __m256i*)pColor);
				my1 = _mm256_add_ps(my1, AvxSupport::wordToPackedFloat(_mm256_extracti128_si256(colorValue, 0)));
				my2 = _mm256_add_ps(my2, AvxSupport::wordToPackedFloat(_mm256_extracti128_si256(colorValue, 1)));
*/
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
/*					const __m256i colorValue = _mm256_loadu_si256((const __m256i*)pColor);
					const __m256 d1 = _mm256_sub_ps(AvxSupport::wordToPackedFloat(_mm256_extracti128_si256(colorValue, 0)), my1);
					const __m256 d2 = _mm256_sub_ps(AvxSupport::wordToPackedFloat(_mm256_extracti128_si256(colorValue, 1)), my2);
*/
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
//					const __m256i colorValue = _mm256_loadu_si256((const __m256i*)pColor);
//					const __m256 lo8 = AvxSupport::wordToPackedFloat(_mm256_extracti128_si256(colorValue, 0));
//					const __m256 hi8 = AvxSupport::wordToPackedFloat(_mm256_extracti128_si256(colorValue, 1));
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
				my += static_cast<float>(*pColor);
			}
			my /= N;

			for (int iteration = 0; iteration < nIterations; ++iteration)
			{
				float S{ 0.0f };

				// Calculate sigma� related to � of last iteration.
				for (auto frameAddress : lineAddresses)
				{
					const T *const pColor = static_cast<T*>(frameAddress) + n + colorOffset;
					const float d = static_cast<float>(*pColor) - my;
					S += (d * d);
				}
				const float sigmaSq = S / N;

				// Calculate new � using current sigma�.
				float W{ 0.0f };
				S = 0.0f;
				for (auto frameAddress : lineAddresses)
				{
					const T *const pColor = static_cast<T*>(frameAddress) + n + colorOffset;
					const float color = static_cast<float>(*pColor);
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
#endif