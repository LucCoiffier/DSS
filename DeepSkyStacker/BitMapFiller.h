#pragma once

#include "BitmapBase.h"
#include "DSSProgress.h"
#include <memory>


class BitmapFillerInterface
{
public:
	static std::unique_ptr<BitmapFillerInterface> makeBitmapFiller(CMemoryBitmap* pBitmap, CDSSProgress* pProgress, const double redWb, const double greenWb, const double blueWb);
	virtual ~BitmapFillerInterface() {}

	virtual bool isThreadSafe() const;
	virtual std::unique_ptr<BitmapFillerInterface> clone() = 0;

	virtual void SetCFAType(CFATYPE CFAType) = 0;
	virtual void setGrey(bool grey) = 0;
	virtual void setWidth(LONG width) = 0;
	virtual void setHeight(LONG height) = 0;
	virtual void setMaxColors(LONG maxcolors) = 0;
	virtual size_t Write(const void* source, const size_t size, const size_t count, const size_t rowIndex) = 0;
};

class BitmapFillerBase : public BitmapFillerInterface
{
protected:
	CDSSProgress* pProgress;
	CMemoryBitmap* pBitmap;
	const float redScale;
	const float greenScale;
	const float blueScale;
	CFATYPE cfaType;
	bool isGray;
	int width;
	int height;
	int bytesPerChannel;
	std::vector<float> redBuffer;
	std::vector<float> greenBuffer;
	std::vector<float> blueBuffer;
	std::vector<float> cfaFactors;
public:
	BitmapFillerBase(CMemoryBitmap* pB, CDSSProgress* pP, const double redWb, const double greenWb, const double blueWb);
	virtual ~BitmapFillerBase() {}

	virtual void SetCFAType(CFATYPE cfaType) override;
	virtual void setGrey(bool grey) override;
	virtual void setWidth(LONG width) override;
	virtual void setHeight(LONG height) override;
	virtual void setMaxColors(LONG maxcolors) override;
protected:
	void setCfaFactors();
	bool isRgbBayerPattern() const;

	inline static float adjustColor(const float color, const float adjustFactor)
	{
		constexpr float Maximum = static_cast<float>(std::numeric_limits<std::uint16_t>::max() - 1);
		return std::min(color * adjustFactor, Maximum);
	};
};
