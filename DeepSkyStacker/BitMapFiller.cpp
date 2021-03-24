#include "StdAfx.h"
#include "BitMapFiller.h"
#include "avx_bitmap_filler.h"
#include "avx_support.h"

// ----------------------------------
// BitmapFillerInterface
// ----------------------------------

std::unique_ptr<BitmapFillerInterface> BitmapFillerInterface::makeBitmapFiller(CMemoryBitmap* pBitmap, CDSSProgress* pProgress, const double redWb, const double greenWb, const double blueWb)
{
	if (AvxSupport::checkSimdAvailability())
		return std::make_unique<AvxBitmapFiller>(pBitmap, pProgress, redWb, greenWb, blueWb);
	else
		return std::make_unique<NonAvxBitmapFiller>(pBitmap, pProgress, redWb, greenWb, blueWb);
}

bool BitmapFillerInterface::isThreadSafe() const { return false; }


// ----------------------------------
// BitmapFillerBase
// ----------------------------------

BitmapFillerBase::BitmapFillerBase(CMemoryBitmap* pB, CDSSProgress* pP, const double redWb, const double greenWb, const double blueWb) :
	pBitmap{ pB },
	pProgress{ pP },
	redScale{ static_cast<float>(redWb) },
	greenScale{ static_cast<float>(greenWb) },
	blueScale{ static_cast<float>(blueWb) },

	cfaType{ CFATYPE_NONE },
	isGray{ true },
	width{ 0 },
	height{ 0 },
	bytesPerChannel{ 0 },
	redBuffer{},
	greenBuffer{},
	blueBuffer{},
	cfaFactors{ 1.0f, 1.0f, 1.0f, 1.0f }
{}

void BitmapFillerBase::SetCFAType(CFATYPE cfaTp)
{
	this->cfaType = cfaTp;
	if (auto* pGray16Bitmap = dynamic_cast<C16BitGrayBitmap*>(pBitmap))
		pGray16Bitmap->SetCFAType(cfaTp);
	setCfaFactors();
}

void BitmapFillerBase::setCfaFactors()
{
	const auto setFactors = [this](const float f0, const float f1, const float f2, const float f3) -> void
	{
		this->cfaFactors.assign({ f0, f1, f2, f3 });
	};

	switch (cfaType)
	{
	case CFATYPE_BGGR: return setFactors(blueScale, greenScale, greenScale, redScale);
	case CFATYPE_GRBG: return setFactors(greenScale, redScale, blueScale, greenScale);
	case CFATYPE_GBRG: return setFactors(greenScale, blueScale, redScale, greenScale);
	case CFATYPE_RGGB: return setFactors(redScale, greenScale, greenScale, blueScale);
	}
};

bool BitmapFillerBase::isRgbBayerPattern() const
{
	switch (this->cfaType)
	{
	case CFATYPE_BGGR:
	case CFATYPE_GRBG:
	case CFATYPE_GBRG:
	case CFATYPE_RGGB: return true;
	}
	return false;
}

void BitmapFillerBase::setGrey(bool grey)
{
	this->isGray = grey;
}

void BitmapFillerBase::setWidth(LONG width)
{
	this->width = width;
}

void BitmapFillerBase::setHeight(LONG height)
{
	this->height = height;
	if (pProgress != nullptr)
		pProgress->Start2(nullptr, pBitmap->Height());
}

void BitmapFillerBase::setMaxColors(LONG maxcolors)
{
	this->bytesPerChannel = maxcolors > 255 ? 2 : 1;
}


// ---------------------------------
// Non-AVX Bitmap Filler
// ---------------------------------

NonAvxBitmapFiller::NonAvxBitmapFiller(CMemoryBitmap* pB, CDSSProgress* pP, const double redWb, const double greenWb, const double blueWb) :
	BitmapFillerBase{ pB, pP, redWb, greenWb, blueWb }
{}

bool NonAvxBitmapFiller::isThreadSafe() const { return true; }

std::unique_ptr<BitmapFillerInterface> NonAvxBitmapFiller::clone()
{
	return std::make_unique<NonAvxBitmapFiller>(*this);
}

size_t NonAvxBitmapFiller::Write(const void* source, const size_t bytesPerPixel, const size_t nrPixels, const size_t rowIndex)
{
	ZASSERTSTATE(0 != this->width);
	ZASSERTSTATE(0 != this->height);
	ZASSERTSTATE(0 != this->bytesPerChannel);
	ZASSERTSTATE((nrPixels % static_cast<size_t>(this->width)) == 0);

	if (this->isGray)
	{
		ZASSERTSTATE(bytesPerPixel == this->bytesPerChannel);
		constexpr size_t vectorLen = 16;
		redBuffer.resize(nrPixels);

		if (this->bytesPerChannel == 1)
		{
			const std::uint8_t* const pData = static_cast<const std::uint8_t*>(source);
			for (size_t i = 0; i < nrPixels; ++i)
				redBuffer[i] = static_cast<float>(static_cast<std::uint16_t>(pData[i]) << 8);
		}
		else
		{
			const std::uint16_t* const pData = static_cast<const std::uint16_t*>(source);
			for (size_t i = 0; i < nrPixels; ++i)
				redBuffer[i] = static_cast<float>(_byteswap_ushort(pData[i])); // Load an convert to little endian
		}

		if (this->isRgbBayerPattern())
		{
			const size_t y = 2 * (rowIndex % 2); // 0, 2, 0, 2, ...
			const float adjustFactors[2] = { this->cfaFactors[y], this->cfaFactors[y + 1] }; // {0, 1} or {2, 3}, depending on the line number.
			for (size_t i = 0; i < nrPixels; ++i)
				redBuffer[i] = adjustColor(redBuffer[i], adjustFactors[i % 2]);
		}

		auto* pGray16Bitmap = dynamic_cast<C16BitGrayBitmap*>(pBitmap);
		ZASSERTSTATE(pGray16Bitmap != nullptr);
		std::uint16_t* const pOut = pGray16Bitmap->m_vPixels.data() + rowIndex * nrPixels;
		for (size_t i = 0; i < nrPixels; ++i)
			pOut[i] = static_cast<std::uint16_t>(redBuffer[i]);
	}
	else
	{
		ZASSERTSTATE(bytesPerPixel == this->bytesPerChannel * 3);
		redBuffer.resize(nrPixels);
		greenBuffer.resize(nrPixels);
		blueBuffer.resize(nrPixels);

		if (this->bytesPerChannel == 1)
		{
			const std::uint8_t* pData = static_cast<const std::uint8_t*>(source);
			for (size_t i = 0; i < nrPixels; ++i, pData += 3)
			{
				redBuffer[i] = static_cast<float>(static_cast<std::uint16_t>(pData[0]) << 8);
				greenBuffer[i] = static_cast<float>(static_cast<std::uint16_t>(pData[1]) << 8);
				blueBuffer[i] = static_cast<float>(static_cast<std::uint16_t>(pData[2]) << 8);
			}
		}
		else
		{
			const std::uint16_t* pData = static_cast<const std::uint16_t*>(source);
			for (size_t i = 0; i < nrPixels; ++i, pData += 3)
			{
				redBuffer[i] = static_cast<float>(_byteswap_ushort(pData[0]));
				greenBuffer[i] = static_cast<float>(_byteswap_ushort(pData[1]));
				blueBuffer[i] = static_cast<float>(_byteswap_ushort(pData[2]));
			}
		}

		std::for_each(redBuffer.begin(), redBuffer.end(), [this](float& v) { v = adjustColor(v, this->redScale); });
		std::for_each(greenBuffer.begin(), greenBuffer.end(), [this](float& v) { v = adjustColor(v, this->greenScale); });
		std::for_each(blueBuffer.begin(), blueBuffer.end(), [this](float& v) { v = adjustColor(v, this->blueScale); });

		auto* pColor16Bitmap = dynamic_cast<C48BitColorBitmap*>(pBitmap);
		ZASSERTSTATE(pColor16Bitmap != nullptr);
		std::uint16_t* const pOutRed = pColor16Bitmap->m_Red.m_vPixels.data() + rowIndex * nrPixels;
		std::uint16_t* const pOutGreen = pColor16Bitmap->m_Green.m_vPixels.data() + rowIndex * nrPixels;
		std::uint16_t* const pOutBlue = pColor16Bitmap->m_Blue.m_vPixels.data() + rowIndex * nrPixels;
		for (size_t i = 0; i < nrPixels; ++i)
		{
			pOutRed[i] = static_cast<std::uint16_t>(redBuffer[i]);
			pOutGreen[i] = static_cast<std::uint16_t>(greenBuffer[i]);
			pOutBlue[i] = static_cast<std::uint16_t>(blueBuffer[i]);
		}
	}

	//if (((rowIndex + 1) % 32) == 0 && this->pProgress != nullptr)
	//	this->pProgress->Progress2(nullptr, static_cast<LONG>(rowIndex + 1));

	return nrPixels;
}
