#pragma once

#include "avx_support.h" 
#include <vector>

class AvxHistogram
{
public:
	typedef std::vector<long> HistogramVectorType;
private:
	HistogramVectorType redHisto;
	HistogramVectorType greenHisto;
	HistogramVectorType blueHisto;
	CMemoryBitmap& inputBitmap;
	bool avxReady;
	bool allRunsSuccessful;
public:
	AvxHistogram() = delete;
	AvxHistogram(CMemoryBitmap& inputbm);
	AvxHistogram(const AvxHistogram&) = default;
	AvxHistogram(AvxHistogram&&) = delete;
	AvxHistogram& operator=(const AvxHistogram&) = delete;

	int calcHistogram(const size_t lineStart, const size_t lineEnd);
	int mergeHistograms(HistogramVectorType& red, HistogramVectorType& green, HistogramVectorType& blue);
	bool histogramSuccessful() const;
private:
	static constexpr size_t HistogramSize() { return std::numeric_limits<std::uint16_t>::max() + size_t{1}; }
	template <class T>
	int doCalcHistogram(const size_t lineStart, const size_t lineEnd);

	// Conflict detection: Number of equal elements + blocking mask.
	inline static std::tuple<__m256i, std::uint32_t> detectConflictsEpi32(const __m256i a) noexcept
	{
		__m256i counter = _mm256_set1_epi32(1);
		std::uint32_t bitMask = 0;

		__m256i shifted = AvxSupport::shiftRightEpi32<1>(a);
		__m256i c = _mm256_blend_epi32(_mm256_cmpeq_epi32(a, shifted), _mm256_setzero_si256(), 0x80); // Set highest mask to "no conflict" (=0) 
		counter = _mm256_sub_epi32(counter, c); // Add one where there is a conflict with the element left from it (by one position). 
		bitMask |= (_mm256_movemask_epi8(c) << 4); // If there is a conflict with the element left from it -> set the mask of that element to 1. 

		shifted = AvxSupport::shiftRightEpi32<1>(shifted);
		c = _mm256_blend_epi32(_mm256_cmpeq_epi32(a, shifted), _mm256_setzero_si256(), 0xc0);
		counter = _mm256_sub_epi32(counter, c);
		bitMask |= (_mm256_movemask_epi8(c) << 8);

		shifted = AvxSupport::shiftRightEpi32<1>(shifted);
		c = _mm256_blend_epi32(_mm256_cmpeq_epi32(a, shifted), _mm256_setzero_si256(), 0xe0);
		counter = _mm256_sub_epi32(counter, c);
		bitMask |= (_mm256_movemask_epi8(c) << 12);

		shifted = AvxSupport::shiftRightEpi32<1>(shifted);
		c = _mm256_blend_epi32(_mm256_cmpeq_epi32(a, shifted), _mm256_setzero_si256(), 0xf0);
		counter = _mm256_sub_epi32(counter, c);
		bitMask |= (_mm256_movemask_epi8(c) << 16);

		shifted = AvxSupport::shiftRightEpi32<1>(shifted);
		c = _mm256_blend_epi32(_mm256_cmpeq_epi32(a, shifted), _mm256_setzero_si256(), 0xf8);
		counter = _mm256_sub_epi32(counter, c);
		bitMask |= (_mm256_movemask_epi8(c) << 20);

		shifted = AvxSupport::shiftRightEpi32<1>(shifted);
		c = _mm256_blend_epi32(_mm256_cmpeq_epi32(a, shifted), _mm256_setzero_si256(), 0xfc);
		counter = _mm256_sub_epi32(counter, c);
		bitMask |= (_mm256_movemask_epi8(c) << 24);

		shifted = AvxSupport::shiftRightEpi32<1>(shifted);
		c = _mm256_blend_epi32(_mm256_cmpeq_epi32(a, shifted), _mm256_setzero_si256(), 0xfe);
		counter = _mm256_sub_epi32(counter, c);
		bitMask |= (_mm256_movemask_epi8(c) << 28);

		return { counter, bitMask };
	}
public:
	template <class T>
	inline static void calcHistoOfVectorEpi32(const __m256i colorVec, T& histogram)
	{
		const auto [nrEqualColors, bitMask] = detectConflictsEpi32(colorVec);

		const __m256i sourceHisto = _mm256_i32gather_epi32((const int*)&*histogram.begin(), colorVec, 4);
		const __m256i updatedHisto = _mm256_add_epi32(sourceHisto, nrEqualColors);

		if ((bitMask & 1) == 0) // No conflict 
			histogram[_mm256_cvtsi256_si32(colorVec)] = _mm256_cvtsi256_si32(updatedHisto);
		if ((bitMask & (1 << 4)) == 0)
			histogram[_mm256_extract_epi32(colorVec, 1)] = _mm256_extract_epi32(updatedHisto, 1);
		if ((bitMask & (1 << 8)) == 0)
			histogram[_mm256_extract_epi32(colorVec, 2)] = _mm256_extract_epi32(updatedHisto, 2);
		if ((bitMask & (1 << 12)) == 0)
			histogram[_mm256_extract_epi32(colorVec, 3)] = _mm256_extract_epi32(updatedHisto, 3);
		const __m128i colorHiLane = _mm256_extracti128_si256(colorVec, 1);
		const __m128i histoHiLane = _mm256_extracti128_si256(updatedHisto, 1);
		if ((bitMask & (1 << 16)) == 0)
			histogram[_mm_cvtsi128_si32(colorHiLane)] = _mm_cvtsi128_si32(histoHiLane);
		if ((bitMask & (1 << 20)) == 0)
			histogram[_mm_extract_epi32(colorHiLane, 1)] = _mm_extract_epi32(histoHiLane, 1);
		if ((bitMask & (1 << 24)) == 0)
			histogram[_mm_extract_epi32(colorHiLane, 2)] = _mm_extract_epi32(histoHiLane, 2);
		if ((bitMask & (1 << 28)) == 0)
			histogram[_mm_extract_epi32(colorHiLane, 3)] = _mm_extract_epi32(histoHiLane, 3);
	};

	template <class H, class T>
	inline static void addToHisto(H& histo, T grayValue)
	{
		if constexpr (std::is_same<T, double>::value)
			grayValue *= 256.0;
		if constexpr (std::is_integral<T>::value && sizeof(T) == 4) // 32 bit integral type 
			grayValue >>= 16;
		++histo[std::min(static_cast<size_t>(grayValue), size_t{ USHORT_MAX })];
	};
};
