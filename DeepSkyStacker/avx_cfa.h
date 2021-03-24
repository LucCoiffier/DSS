#pragma once

#include "BitmapBase.h"
#include <vector>

class AvxCfaProcessing
{
private:
	typedef __m256i VectorElementType;
	typedef std::vector<VectorElementType> VectorType;

	VectorType redPixels;
	VectorType greenPixels;
	VectorType bluePixels;
	CMemoryBitmap& inputBitmap;
	size_t vectorsPerLine;
public:
	AvxCfaProcessing() = delete;
	AvxCfaProcessing(const size_t lineStart, const size_t lineEnd, CMemoryBitmap& inputbm);
	AvxCfaProcessing(const AvxCfaProcessing&) = default;
	AvxCfaProcessing(AvxCfaProcessing&&) = delete;
	AvxCfaProcessing& operator=(const AvxCfaProcessing&) = delete;

	void init(const size_t lineStart, const size_t lineEnd);
	int interpolate(const size_t lineStart, const size_t lineEnd, const long pixelSizeMultiplier);

	inline size_t nrVectorsPerLine() const { return this->vectorsPerLine; }

	template <class T>
	inline const T* redCfaLine(const size_t rowIndex) const
	{
		if constexpr (std::is_same<T, WORD>::value)
			return reinterpret_cast<const WORD*>(&this->redPixels[rowIndex * vectorsPerLine]);
		else
			return nullptr;
	}
	template <class T>
	inline const T* greenCfaLine(const size_t rowIndex) const
	{
		if constexpr (std::is_same<T, WORD>::value)
			return reinterpret_cast<const WORD*>(&this->greenPixels[rowIndex * vectorsPerLine]);
		else
			return nullptr;
	}
	template <class T>
	inline const T* blueCfaLine(const size_t rowIndex) const
	{
		if constexpr (std::is_same<T, WORD>::value)
			return reinterpret_cast<const WORD*>(&this->bluePixels[rowIndex * vectorsPerLine]);
		else
			return nullptr;
	}

	inline const VectorElementType* redCfaBlock() const { return &*this->redPixels.begin(); }
	inline const VectorElementType* greenCfaBlock() const { return &*this->greenPixels.begin(); }
	inline const VectorElementType* blueCfaBlock() const { return &*this->bluePixels.begin(); }
private:
	int interpolateGrayCFA2Color(const size_t lineStart, const size_t lineEnd);
	WORD* redCfaLine(const size_t rowIndex) { return const_cast<WORD*>(static_cast<const AvxCfaProcessing*>(this)->redCfaLine<WORD>(rowIndex)); }
	WORD* greenCfaLine(const size_t rowIndex) { return const_cast<WORD*>(static_cast<const AvxCfaProcessing*>(this)->greenCfaLine<WORD>(rowIndex)); }
	WORD* blueCfaLine(const size_t rowIndex) { return const_cast<WORD*>(static_cast<const AvxCfaProcessing*>(this)->blueCfaLine<WORD>(rowIndex)); }
};
