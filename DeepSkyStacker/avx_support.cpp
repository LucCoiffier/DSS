#include "StdAfx.h"
#include "avx_support.h"
#include <immintrin.h>


AvxSupport::AvxSupport(CMemoryBitmap& b) noexcept :
	bitmap{ b }
{};

int AvxSupport::getNrChannels() const
{
	CBitmapCharacteristics bitmapCharacteristics;
	const_cast<CMemoryBitmap&>(bitmap).GetCharacteristics(bitmapCharacteristics);
	return bitmapCharacteristics.m_lNrChannels;
};

bool AvxSupport::isColorBitmap() const
{
	return getNrChannels() == 3;
};

template <class T>
bool AvxSupport::isColorBitmapOfType() const
{
	auto* const p = const_cast<AvxSupport*>(this)->getColorPtr<T>();
	const bool isColor = p != nullptr && p->isTopDown();
	if constexpr (std::is_same<T, float>::value)
		return isColor && p->IsFloat() && p->GetMultiplier() == 256.0;
	else
		return isColor;
}

bool AvxSupport::isMonochromeBitmap() const
{
	return getNrChannels() == 1;
};

template <class T>
bool AvxSupport::isMonochromeBitmapOfType() const
{
	if (auto* const p = const_cast<AvxSupport*>(this)->getGrayPtr<T>())
	{
		// Note that Monochrome bitmaps are always topdown -> no extra check required! CF. CGrayBitmap::GetOffset().
		if constexpr (std::is_same<T, float>::value)
			return (p->IsFloat() && !p->IsCFA() && p->GetMultiplier() == 256.0);
		if constexpr (std::is_same<T, WORD>::value)
			return (!p->IsCFA() || isMonochromeCfaBitmapOfType<WORD>());
		return !p->IsCFA();
	}
	return false;
}

template <class T>
bool AvxSupport::isMonochromeCfaBitmapOfType() const
{
	// CFA only supported for T=WORD
	if constexpr (std::is_same<T, WORD>::value)
	{
		auto* const pGray = const_cast<AvxSupport*>(this)->getGrayPtr<T>();
		// We support CFA only for RGGB Bayer matrices with BILINEAR interpolation and no offsets.
		return (pGray != nullptr && pGray->IsCFA() && pGray->GetCFATransformation() == CFAT_BILINEAR && pGray->GetCFAType() == CFATYPE_RGGB && pGray->xOffset() == 0 && pGray->yOffset() == 0);
	}
	else
		return false;
};

bool AvxSupport::isColorBitmapOrCfa() const
{
	return isColorBitmap() || isMonochromeCfaBitmapOfType<WORD>();
}

const int AvxSupport::width() const {
	return bitmap.Width();
}

template <class T>
bool AvxSupport::bitmapHasCorrectType() const
{
	return (isColorBitmapOfType<T>() || isMonochromeBitmapOfType<T>());
}

bool AvxSupport::checkAvx2CpuSupport() noexcept
{
	int cpuid[4] = { -1 };
	// FMA Flag
	__cpuidex(cpuid, 1, 0);
	const bool FMAsupported = ((cpuid[2] & 0x01000) != 0);
	// OS supports AVX (YMM registers)
	const bool OSXSAVEsupported = ((cpuid[2] & (1 << 27)) != 0);
	const bool AVXenabledInOS = ((_xgetbv(0) & 6) == 6); // 6 = SSE (0x2) + YMM (0x4)
	// AVX2 Flag
	__cpuidex(cpuid, 7, 0);
	const bool AVX2supported = ((cpuid[1] & 0x020) != 0);

	//const bool BMI1supported = ((cpuid[1] & 0x04) != 0);
	//const bool BMI2supported = ((cpuid[1] & 0x0100) != 0);

	// Additionally set flush to zero and denormals to zero. 
	_mm_setcsr(_mm_getcsr() | _MM_FLUSH_ZERO_ON | _MM_DENORMALS_ZERO_ON);

	return (FMAsupported && AVX2supported && OSXSAVEsupported && AVXenabledInOS);
};

bool AvxSupport::checkSimdAvailability() noexcept
{
	// If user has disabled SIMD vectorisation (settings dialog) -> return false;
	return CMultitask::GetUseSimd() && checkAvx2CpuSupport();
}

// Explicit template instantiation for the types we need.
template bool AvxSupport::bitmapHasCorrectType<WORD>() const;
template bool AvxSupport::bitmapHasCorrectType<std::uint32_t>() const;
template bool AvxSupport::bitmapHasCorrectType<unsigned long>() const;
template bool AvxSupport::bitmapHasCorrectType<float>() const;
template bool AvxSupport::bitmapHasCorrectType<double>() const;

template bool AvxSupport::isMonochromeCfaBitmapOfType<WORD>() const;
template bool AvxSupport::isMonochromeCfaBitmapOfType<std::uint32_t>() const;
template bool AvxSupport::isMonochromeCfaBitmapOfType<unsigned long>() const;
template bool AvxSupport::isMonochromeCfaBitmapOfType<float>() const;
template bool AvxSupport::isMonochromeCfaBitmapOfType<double>() const;
