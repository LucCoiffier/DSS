#include "StdAfx.h" 
#include "avx_entropy.h" 
#include "avx_support.h" 
#include "avx_cfa.h" 
#include "avx_histogram.h" 
#include <immintrin.h> 
#include <omp.h> 

AvxEntropy::AvxEntropy(CMemoryBitmap& inputbm, const CEntropyInfo& entrinfo, CMemoryBitmap* entropycov) :
	inputBitmap{ inputbm },
	entropyInfo{ entrinfo },
	pEntropyCoverage{ entropycov },
	avxReady{ AvxSupport::checkSimdAvailability() }
{
	if (pEntropyCoverage != nullptr && avxReady)
	{
		const size_t width = pEntropyCoverage->Width();
		const size_t height = pEntropyCoverage->Height();
		static_assert(std::is_same_v<EntropyLayerVectorType::value_type, __m256> && std::is_same_v<EntropyVectorType::value_type, float>);
		const size_t nrVectors = AvxSupport::numberOfAvxVectors<float, __m256>(width);
		redEntropyLayer.resize(height * nrVectors);
		if (AvxSupport{ *pEntropyCoverage }.isColorBitmap())
		{
			greenEntropyLayer.resize(height * nrVectors);
			blueEntropyLayer.resize(height * nrVectors);
		}
	}
}

int AvxEntropy::calcEntropies(const int squareSize, const int nSquaresX, const int nSquaresY, EntropyVectorType& redEntropies, EntropyVectorType& greenEntropies, EntropyVectorType& blueEntropies)
{
	if (!avxReady)
		return 1;

	int rval = 1;
	if (doCalcEntropies<WORD>(squareSize, nSquaresX, nSquaresY, redEntropies, greenEntropies, blueEntropies) == 0
		|| doCalcEntropies<unsigned long>(squareSize, nSquaresX, nSquaresY, redEntropies, greenEntropies, blueEntropies) == 0
		|| doCalcEntropies<float>(squareSize, nSquaresX, nSquaresY, redEntropies, greenEntropies, blueEntropies) == 0)
	{
		rval = 0;
	}
	return AvxSupport::zeroUpper(rval);
}

template <class T>
int AvxEntropy::doCalcEntropies(const int squareSize, const int nSquaresX, const int nSquaresY, EntropyVectorType& redEntropies, EntropyVectorType& greenEntropies, EntropyVectorType& blueEntropies)
{
	// Check input bitmap. 
	const AvxSupport avxInputSupport{ inputBitmap };
	if (!avxInputSupport.isColorBitmapOfType<T>() && !avxInputSupport.isMonochromeBitmapOfType<T>()) // Monochrome includes CFA 
		return 1;

	constexpr int vectorLen = 16;
	const int width = inputBitmap.Width();
	const int height = inputBitmap.Height();

	const auto getDistribution = [](const auto& histogram, T value) -> float
	{
		if constexpr (std::is_integral<T>::value && sizeof(T) == 4) // 32 bit integral type 
			value >>= 16;
		return static_cast<float>(histogram[std::min(static_cast<size_t>(value), size_t{ USHORT_MAX })]);
	};

	const auto calcEntropyOfSquare = [squareSize, width, height, vectorLen, &getDistribution](const int col, const int row, const T* const pColor, auto& histogram) -> EntropyVectorType::value_type
	{
		const int xmin = col * squareSize;
		const int xmax = std::min(xmin + squareSize, width);
		const int nx = xmax - xmin;
		const int ymin = row * squareSize;
		const int ymax = std::min(ymin + squareSize, height);
		const int nrVectors = nx / vectorLen;
		memset(histogram.data(), 0, histogram.size() * sizeof(histogram[0]));

		for (int y = ymin; y < ymax; ++y)
		{
			const T* p = pColor + y * width + xmin;
			for (int n = 0; n < nrVectors; ++n, p += vectorLen)
			{
				const auto [lo, hi] = AvxSupport::read16PackedInt(p);
				AvxHistogram::calcHistoOfVectorEpi32(lo, histogram);
				AvxHistogram::calcHistoOfVectorEpi32(hi, histogram);
			}
			// Rest of line
			for (int x = xmin + nrVectors * vectorLen; x < xmax; ++x, ++p)
				AvxHistogram::addToHisto(histogram, *p);
		}

		const float N = static_cast<float>(nx * (ymax - ymin));
		const float lnN = std::log(N);
		float entropy = 0.0f;
		__m256 avxEntropy = _mm256_setzero_ps();
		const int* const pHisto = histogram.data();

		for (int y = ymin; y < ymax; ++y)
		{
			const T* p = pColor + y * width + xmin;
			for (int n = 0; n < nrVectors; ++n, p += vectorLen)
			{
				const auto [lo, hi] = AvxSupport::read16PackedInt(p);
				const __m256 lh = _mm256_cvtepi32_ps(_mm256_i32gather_epi32(pHisto, lo, 4));
				const __m256 hh = _mm256_cvtepi32_ps(_mm256_i32gather_epi32(pHisto, hi, 4));
				const __m256 r0 = _mm256_fmadd_ps(lh, _mm256_sub_ps(_mm256_set1_ps(lnN), _mm256_log_ps(lh)), avxEntropy);
				avxEntropy = _mm256_fmadd_ps(hh, _mm256_sub_ps(_mm256_set1_ps(lnN), _mm256_log_ps(hh)), r0);
			}
			// Rest of line adds to float entropy.
			for (int x = xmin + nrVectors * vectorLen; x < xmax; ++x, ++p)
			{
				const float d = getDistribution(histogram, *p);
				entropy += d * (lnN - std::log(d));
			}
		}
		// Accumulate float entropy and horizontal sum of avxEntropy.
		const __m256 r0 = _mm256_hadd_ps(_mm256_hadd_ps(avxEntropy, _mm256_setzero_ps()), _mm256_setzero_ps()); // ., ., ., e4+e5+e6+e7, ., ., ., e0+e1+e2+e3
		entropy += _mm_cvtss_f32(_mm_add_ps(_mm256_castps256_ps128(r0), _mm256_extractf128_ps(r0, 1)));
		
		return entropy / (N * std::log(2.0f));
	};

	const auto calcEntropy = [nSquaresX, nSquaresY, &calcEntropyOfSquare](const T* const pColor, EntropyVectorType& entropyVector) -> void
	{
		const int nrEnabledThreads = CMultitask::GetNrProcessors(false); // Returns 1 if multithreading disabled by user, otherwise # HW threads 
		constexpr size_t HistoSize = std::numeric_limits<std::uint16_t>::max() + size_t{ 1 };
		std::vector<int> histogram(HistoSize, 0);

#pragma omp parallel for default(none) firstprivate(histogram) schedule(dynamic, 10) if(nrEnabledThreads - 1) 
		for (int y = 0; y < nSquaresY; ++y)
		{
			for (int x = 0, ndx = y * nSquaresX; x < nSquaresX; ++x, ++ndx)
			{
				entropyVector[ndx] = calcEntropyOfSquare(x, y, pColor, histogram);
			}
		}
	};

	const bool isCFA = avxInputSupport.isMonochromeCfaBitmapOfType<T>();

	if (avxInputSupport.isColorBitmapOfType<T>() || isCFA)
	{
		AvxCfaProcessing avxCfa{ 0, 0, inputBitmap };
		if (isCFA)
		{
			const size_t lineEnd = inputBitmap.Height();
			avxCfa.init(0, lineEnd);
			avxCfa.interpolate(0, lineEnd, 1);
		}

		const T* pRedPixels = isCFA ? avxCfa.redCfaLine<T>(0) : &avxInputSupport.redPixels<T>().at(0);
		const T* pGreenPixels = isCFA ? avxCfa.greenCfaLine<T>(0) : &avxInputSupport.greenPixels<T>().at(0);
		const T* pBluePixels = isCFA ? avxCfa.blueCfaLine<T>(0) : &avxInputSupport.bluePixels<T>().at(0);

		calcEntropy(pRedPixels, redEntropies);
		calcEntropy(pGreenPixels, greenEntropies);
		calcEntropy(pBluePixels, blueEntropies);

		return 0;
	}

	if (avxInputSupport.isMonochromeBitmapOfType<T>())
	{
		const T* pGrayPixels = &avxInputSupport.grayPixels<T>().at(0);
		calcEntropy(pGrayPixels, redEntropies);

		memcpy(&greenEntropies[0], &redEntropies[0], redEntropies.size() * sizeof(EntropyVectorType::value_type));
		memcpy(&blueEntropies[0], &redEntropies[0], redEntropies.size() * sizeof(EntropyVectorType::value_type));

		return 0;
	}

	return 1;
}
