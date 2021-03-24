#pragma once

#include "BitMapFiller.h"

class AvxBitmapFiller : public BitmapFillerBase
{
private:
	std::vector<std::uint8_t> sourceBuffer;
public:
	AvxBitmapFiller(CMemoryBitmap* pB, CDSSProgress* pP, const double redWb, const double greenWb, const double blueWb);
	AvxBitmapFiller(const AvxBitmapFiller&) = default; // For cloning.
	virtual ~AvxBitmapFiller() {}

	virtual bool isThreadSafe() const override;
	virtual std::unique_ptr<BitmapFillerInterface> clone() override;

	virtual size_t Write(const void* source, const size_t bytesPerPixel, const size_t nrPixels, const size_t rowIndex) override;
};

class NonAvxBitmapFiller : public BitmapFillerBase
{
public:
	NonAvxBitmapFiller(CMemoryBitmap* pB, CDSSProgress* pP, const double redWb, const double greenWb, const double blueWb);
	NonAvxBitmapFiller(const NonAvxBitmapFiller&) = default; // For cloning.
	virtual ~NonAvxBitmapFiller() {}

	virtual bool isThreadSafe() const override;
	virtual std::unique_ptr<BitmapFillerInterface> clone() override;

	virtual size_t Write(const void* source, const size_t bytesPerPixel, const size_t nrPixels, const size_t rowIndex) override;
};
