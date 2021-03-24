#include "stdafx.h"
#include "catch.h"
#include "../DeepSkyStacker/avx_bitmap_filler.h"
#include <immintrin.h>

template <size_t SZ>
void be2le(std::uint16_t(&out)[SZ], const std::uint16_t* pIn)
{
	for (size_t n = 0; n < SZ; ++n)
//		out[n] = _load_be_u16(pIn + n);
		out[n] = _byteswap_ushort(pIn[n]);
}

// ------------------
// Gray
// ------------------

TEMPLATE_TEST_CASE("BitmapFiller gray", "[Bitmap][BitmapFiller][gray]", AvxBitmapFiller, NonAvxBitmapFiller)
{
	CSmartPtr<CMemoryBitmap> pBitmap;
	pBitmap.Attach(new CGrayBitmapT<WORD>);
	// TestType is the current type from the list of types for which this TEST_CASE is run.
	CopyableSmartPtr<BitmapFillerInterface> filler = std::make_unique<TestType>(pBitmap, nullptr, 1.0, 1.0, 1.0);
	filler->setGrey(true);

	SECTION("Write 1 line 8 bps")
	{
		filler->setMaxColors(255);
		filler->setWidth(18);
		filler->setHeight(1);
		std::uint8_t inputData[18] = { 19, 45, 243, 65, 200, 110, 99, 225,  120, 121, 122, 123, 124, 125, 126, 127,  195, 196 };
		pBitmap->Init(18, 1);

		filler->Write(inputData, 1, 18, 0);

		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		WORD data[18];
		for (int i = 0; i < 18; ++i)
			data[i] = inputData[i] << 8;
		REQUIRE(memcmp(pGray->m_vPixels.data(), data, 36) == 0);
	}

	SECTION("Write 1 line 16 bps")
	{
		filler->setMaxColors(65535);
		filler->setWidth(19);
		filler->setHeight(1);
		std::uint16_t inputData[19] = { 1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007,  2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 49028, 57215, 60001 };
		be2le(inputData, inputData); // Make it big endian
		pBitmap->Init(19, 1);

		filler->Write(inputData, 2, 19, 0);

		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		WORD data[19];
		be2le(data, inputData);
		REQUIRE(memcmp(pGray->m_vPixels.data(), data, 16) == 0);
	}

	SECTION("Write 2 lines 16 bps with 2 calls")
	{
		filler->setMaxColors(65535);
		filler->setWidth(8);
		filler->setHeight(2);
		std::uint16_t inputData[16] = { 1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007 };
		pBitmap->Init(8, 2);

		filler->Write(inputData, 2, 8, 0);
		filler->Write(inputData + 8, 2, 8, 1);

		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		WORD data[16];
		be2le(data, inputData);

		REQUIRE(memcmp(pGray->m_vPixels.data(), data, 32) == 0);
	}

	SECTION("2 lines 16 bps and adjust RGGB (this must set the CFA type of the MemoryBitmap")
	{
		constexpr size_t W = 17;
		auto filler = std::make_unique<TestType>(pBitmap, nullptr, 2.0, 3.0, 4.0);
		filler->setMaxColors(65535);
		filler->setWidth(W);
		filler->setHeight(2);
		filler->SetCFAType(CFATYPE_RGGB);
		std::uint16_t inputData[W * 2] = { 256, 257, 258, 1003, 1075, 2328, 32767, 20000, 5000, 6000, 7000, 9002, 9003, 9004, 10010, 10100, 10101,
			8245, 8255, 8256, 8257, 8258, 8259, 8295, 8296, 6928, 6129, 1293, 1294, 1299, 6002, 6001, 6007, 3333 };
		be2le(inputData, inputData);
		pBitmap->Init(W, 2);

		filler->Write(inputData, 2, W, 0);
		filler->Write(inputData + W, 2, W, 1);

		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		WORD data[W * 2];
		for (size_t i = 0; i < W; ++i)
			data[i] = _load_be_u16(&inputData[i]) * (i % 2 == 0 ? 2 : 3);
		for (size_t i = 0; i < W; ++i)
			data[i + W] = _load_be_u16(&inputData[i + W]) * (i % 2 == 0 ? 3 : 4);

		auto* pGrayBitmap = dynamic_cast<C16BitGrayBitmap*>(pBitmap.m_p);

		REQUIRE(memcmp(pGray->m_vPixels.data(), data, sizeof(data)) == 0);
		REQUIRE(pGrayBitmap != nullptr);
		REQUIRE(pGrayBitmap->GetCFAType() == CFATYPE_RGGB);
	}

	SECTION("2 lines 16 bps with one call fails")
	{
		constexpr size_t W = 17;
		auto filler = std::make_unique<TestType>(pBitmap, nullptr, 2.0, 3.0, 4.0);
		filler->setMaxColors(65535);
		filler->setWidth(W);
		filler->setHeight(2);
		filler->SetCFAType(CFATYPE_RGGB);
		std::uint16_t inputData[W * 2] = { 256, 257, 258, 1003, 1075, 2328, 32767, 20000, 5000, 6000, 7000, 9002, 9003, 9004, 10010, 10100, 10101,
			8245, 8255, 8256, 8257, 8258, 8259, 8295, 8296, 6928, 6129, 1293, 1294, 1299, 6002, 6001, 6007, 3333 };
		be2le(inputData, inputData);
		pBitmap->Init(W, 2);

		filler->Write(inputData, 2, 2 * W, 0);

		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		WORD data[W * 2];
		for (size_t i = 0; i < W; ++i)
			data[i] = _load_be_u16(&inputData[i]) * (i % 2 == 0 ? 2 : 3);
		for (size_t i = 0; i < W; ++i)
			data[i + W] = _load_be_u16(&inputData[i + W]) * (i % 2 == 0 ? 3 : 4);

		REQUIRE(memcmp(pGray->m_vPixels.data(), data, sizeof(data)) != 0); // Second line (GBGB) incorrectly interpolated, because filler treats it like RGRG...
	}

	SECTION("2 lines 16 bps and adjust different CFA schemes")
	{
		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);

		const auto testCfaType = [&pBitmap, pGray, &filler](const CFATYPE cfaType, WORD wb0, WORD wb1, WORD wb2, WORD wb3) -> int
		{
			constexpr size_t W = 17;
			std::uint16_t inputData[W * 2] = { 256, 257, 258, 1003, 1075, 2328, 32767, 20000, 5000, 6000, 7000, 9002, 9003, 9004, 10010, 10100, 10101,
				8245, 8255, 8256, 8257, 8258, 8259, 8295, 8296, 6928, 6129, 1293, 1294, 1299, 6002, 6001, 6007, 3333 };
			auto fil = std::make_unique<TestType>(pBitmap, nullptr, 2.0, 3.0, 4.0);
			fil->setGrey(true);
			fil->setMaxColors(65535);
			fil->setWidth(W);
			fil->setHeight(2);
			fil->SetCFAType(cfaType);

			std::uint16_t beData[W * 2];
			be2le(beData, inputData);
			pBitmap->Init(W, 2);

			fil->Write(beData, 2, W, 0);
			fil->Write(beData + W, 2, W, 1);

			WORD wb[2][2] = { {wb0, wb1}, {wb2, wb3} };
			WORD expected[W * 2];
			for (size_t y = 0; y < 2; ++y)
				for (size_t x = 0; x < W; ++x)
					expected[y * W + x] = std::min(inputData[y * W + x] * wb[y % 2][x % 2], 65534);
			return memcmp(pGray->m_vPixels.data(), expected, sizeof(expected));
		};

		REQUIRE(testCfaType(CFATYPE_BGGR, 4, 3, 3, 2) == 0);
		REQUIRE(testCfaType(CFATYPE_GRBG, 3, 2, 4, 3) == 0);
		REQUIRE(testCfaType(CFATYPE_GBRG, 3, 4, 2, 3) == 0);
		REQUIRE(testCfaType(CFATYPE_RGGB, 2, 3, 3, 4) == 0);
		// These are CFA types that do not need white balancing, so the coefficients are all 1.
		REQUIRE(testCfaType(CFATYPE_NONE, 1, 1, 1, 1) == 0);
		REQUIRE(testCfaType(CFATYPE_YMGC, 1, 1, 1, 1) == 0);
		REQUIRE(testCfaType(CFATYPE_CYMGCYGM, 1, 1, 1, 1) == 0);
	}

	SECTION("1 line 16 bps adjust green color and limit to maximum")
	{
		constexpr size_t W = 17;
		auto filler = std::make_unique<TestType>(pBitmap, nullptr, 2.0, 10.0, 1.0); // Green factor = 10.
		filler->setMaxColors(65535);
		filler->setWidth(W);
		filler->setHeight(1);
		filler->SetCFAType(CFATYPE_RGGB);
		std::uint16_t inputData[W] = { 2560, 3249, 29265, 50000, 5, 50002, 6, 7000, 7, 6000, 8, 9000, 9, 10000, 10, 10005, 11 }; // R, G, R, G, ..., R
		be2le(inputData, inputData);
		pBitmap->Init(W, 1);

		filler->Write(inputData, 2, W, 0);
		constexpr WORD MAXIMUM = 65534; // For some strange reason, the old BitMapFiller limited this to MAXWORD - 1
		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		WORD data[W] = { 2560 * 2, 3249 * 10, 29265 * 2, MAXIMUM, 5 * 2, MAXIMUM, 6 * 2, MAXIMUM, 7 * 2, 6000 * 10, 8 * 2, MAXIMUM, 9 * 2, MAXIMUM, 10 * 2, MAXIMUM, 11 * 2 };
		REQUIRE(memcmp(pGray->m_vPixels.data(), data, sizeof(data)) == 0);
	}
}

// -------------------
// Color
// -------------------

TEMPLATE_TEST_CASE("BitmapFiller color", "[Bitmap][BitmapFiller][color]", AvxBitmapFiller, NonAvxBitmapFiller)
{
	CSmartPtr<CMemoryBitmap> pBitmap;
	pBitmap.Attach(new CColorBitmapT<WORD>);
	CopyableSmartPtr<BitmapFillerInterface> filler = std::make_unique<TestType>(pBitmap, nullptr, 1.0, 1.0, 1.0);
	filler->setGrey(false);

	SECTION("Write 1 line 8 bps")
	{
		constexpr size_t W = 17;
		filler->setMaxColors(255);
		filler->setWidth(W);
		filler->setHeight(1);
		std::uint8_t inputData[W * 3] = { 19, 20, 21,  45, 46, 47,  243, 244, 245,  65, 66, 67,  200, 201, 202,  110, 111, 112,  99, 100, 101,  225, 226, 227,
			190, 200, 210,  50, 60, 70,  231, 241, 251,  152, 162, 172,  203, 201, 202,  104, 114, 124,  95, 105, 101,  225, 226, 227,  100, 86, 77 }; // r,g,b, r,g,b, ...
		pBitmap->Init(W, 1);

		filler->Write(inputData, 3, W, 0);

		auto* pGray = dynamic_cast<CColorBitmapT<WORD>*>(pBitmap.m_p);
		const WORD* pRed = pGray->m_Red.m_vPixels.data();
		const WORD* pGreen = pGray->m_Green.m_vPixels.data();
		const WORD* pBlue = pGray->m_Blue.m_vPixels.data();
		bool correct = true;
		constexpr WORD twofiftysix = 256;
		for (size_t n = 0; n < W; ++n)
		{
			if (pRed[n] != twofiftysix * inputData[3 * n])
				correct = false;
			if (pGreen[n] != twofiftysix * inputData[3 * n + 1])
				correct = false;
			if (pBlue[n] != twofiftysix * inputData[3 * n + 2])
				correct = false;
		}
		REQUIRE(correct == true);
	}

	SECTION("Write 1 line 16 bps")
	{
		constexpr size_t W = 18;
		filler->setMaxColors(65535);
		filler->setWidth(W);
		filler->setHeight(1);
		std::uint16_t inputData[W * 3] = { 1000, 1001, 1002,  255, 256, 257,  243, 244, 245,  2002, 2003, 2004,  16540, 16796, 17052,  11110, 11111, 11112,  9900, 9901, 9902,  24325, 24326, 24327,
			6190, 6200, 6210,  750, 760, 770,  8231, 8241, 8251,  9152, 9162, 9172,  10203, 10201, 10202,  36104, 36114, 36124,  9563, 10563, 10163,  2257, 2267, 2277,
			10058, 8658, 7758,  85, 58, 137 };
		pBitmap->Init(W, 1);

		filler->Write(inputData, 3 * sizeof(std::uint16_t), W, 0);

		auto* pGray = dynamic_cast<CColorBitmapT<WORD>*>(pBitmap.m_p);
		const WORD* pRed = pGray->m_Red.m_vPixels.data();
		const WORD* pGreen = pGray->m_Green.m_vPixels.data();
		const WORD* pBlue = pGray->m_Blue.m_vPixels.data();

		std::uint16_t data[W * 3];
		be2le(data, inputData);

		bool correct = true;
		for (int n = 0; n < W; ++n)
		{
			if (pRed[n] != data[3 * n])
				correct = false;
			if (pGreen[n] != data[3 * n + 1])
				correct = false;
			if (pBlue[n] != data[3 * n + 2])
				correct = false;
		}
		REQUIRE(correct == true);
	}

	SECTION("Write 2 lines with 2 calls")
	{
		constexpr size_t W = 17;
		filler->setMaxColors(65535);
		filler->setWidth(W);
		filler->setHeight(2);
		std::uint16_t inputData[W * 3 * 2] = { 1000, 1001, 1002,   255, 256, 257,    243, 244, 245,    2002, 2003, 2004,  16540, 16796, 17052,  11110, 11111, 11112,  9900, 9901, 9902,    24325, 24326, 24327,
										3000, 3001, 3002,  2255, 2256, 2257,  2243, 2244, 2245, 4002, 4003, 4004,  18540, 18796, 19052,  13110, 13111, 13112,  11900, 11901, 11902, 26325, 26326, 26327,
										62229, 62231, 62233,
										10001, 10011, 10021,  2552, 2562, 2572,  2433, 2443, 2453,  20024, 20034, 20044, 16544, 16795, 17057,  11130, 11131, 11132,  9950, 9951, 9952,    44325, 44326, 44327,
										30007, 30017, 30027,  22557, 22567, 22577,  22437, 22447, 22457, 40027, 40037, 40047,  8540, 8796, 9052,  33110, 33111, 33112,  21900, 21901, 21902, 26385, 26386, 26387,
										62269, 62261, 62263 };

		pBitmap->Init(W, 2);

		filler->Write(inputData, 3 * 2, W, 0);
		filler->Write(inputData + W * 3, 3 * 2, W, 1);

		auto* pGray = dynamic_cast<CColorBitmapT<WORD>*>(pBitmap.m_p);
		const WORD* pRed = pGray->m_Red.m_vPixels.data();
		const WORD* pGreen = pGray->m_Green.m_vPixels.data();
		const WORD* pBlue = pGray->m_Blue.m_vPixels.data();

		std::uint16_t data[W * 3 * 2];
		be2le(data, inputData);

		bool correct = true;
		for (int n = 0; n < W * 2; ++n)
		{
			if (pRed[n] != data[3 * n])
				correct = false;
			if (pGreen[n] != data[3 * n + 1])
				correct = false;
			if (pBlue[n] != data[3 * n + 2])
				correct = false;
		}
		REQUIRE(correct == true);
	}

	SECTION("Write 1 line 16 bps and adjust colors")
	{
		constexpr size_t W = 18;
		auto filler = std::make_unique<TestType>(pBitmap, nullptr, 0.3, 0.24, 1.18);
		filler->setGrey(false);
		filler->setMaxColors(65535);
		filler->setWidth(W);
		filler->setHeight(1);
		std::uint16_t inputData[W * 3] = { 1000, 1001, 1002,  255, 256, 257,  243, 244, 245,  2002, 2003, 2004,  16540, 16796, 17052,  11110, 11111, 11112,  9900, 9901, 9902,  24325, 24326, 24327,
			6190, 6200, 6210,  750, 760, 770,  8231, 8241, 8251,  9152, 9162, 9172,  10203, 10201, 10202,  36104, 36114, 64000,  9563, 10563, 10163,  2257, 2267, 2277,
			10058, 8658, 7758,  85, 58, 63003 };
		pBitmap->Init(W, 1);

		std::uint16_t beData[W * 3];
		be2le(beData, inputData);
		filler->Write(beData, 3 * 2, W, 0);

		auto* pGray = dynamic_cast<CColorBitmapT<WORD>*>(pBitmap.m_p);
		const WORD* pRed = pGray->m_Red.m_vPixels.data();
		const WORD* pGreen = pGray->m_Green.m_vPixels.data();
		const WORD* pBlue = pGray->m_Blue.m_vPixels.data();

		bool correct = true;
		for (int n = 0; n < W; ++n)
		{
			if (pRed[n] != static_cast<WORD>(std::min(0.3 * static_cast<double>(inputData[3 * n]), 65534.0)))
				correct = false;
			if (pGreen[n] != static_cast<WORD>(std::min(0.24 * static_cast<double>(inputData[3 * n + 1]), 65534.0)))
				correct = false;
			if (pBlue[n] != static_cast<WORD>(std::min(1.18 * static_cast<double>(inputData[3 * n + 2]), 65534.0)))
				correct = false;
		}
		REQUIRE(correct == true);
	}
}


// ---------------
// Progress
// ---------------

class MyProgress : public CDSSProgress
{
public:
	int nrCallsStart2 = 0;
	int nrCallsProgress2 = 0;
	LONG argumentStart2 = -1;
	std::vector<LONG> argumentsAchieved2;
public:
	void GetStartText(CString& strText) {}
	void GetStart2Text(CString& strText) {}
	void Start(LPCTSTR szTitle, LONG lTotal1, bool bEnableCancel = true) {}
	void Progress1(LPCTSTR szText, LONG lAchieved1) {}
	void Start2(LPCTSTR szText, LONG lTotal2) {
		++nrCallsStart2;
		argumentStart2 = lTotal2;
	}
	void Progress2(LPCTSTR szText, LONG lAchieved2) {
		++nrCallsProgress2;
		argumentsAchieved2.push_back(lAchieved2);
	}
	void End2() {}
	bool IsCanceled() { return false; }
	bool Close() { return true; }
};

TEMPLATE_TEST_CASE("BitmapFiller progress check", "[Bitmap][BitmapFiller][progress]", AvxBitmapFiller, NonAvxBitmapFiller)
{
	CSmartPtr<CMemoryBitmap> pBitmap;
	pBitmap.Attach(new CGrayBitmapT<WORD>);
	MyProgress prg{};
	CopyableSmartPtr<BitmapFillerInterface> filler = std::make_unique<TestType>(pBitmap, &prg, 1.0, 1.0, 1.0);
	filler->setGrey(true);

	SECTION("Write 64 lines and check progress calls")
	{
		constexpr size_t H = 64;
		pBitmap->Init(4, H);
		filler->setMaxColors(255);
		filler->setWidth(4);
		filler->setHeight(H);
		std::uint8_t inputData[H * 4];

		for (size_t i = 0; i < sizeof(inputData); ++i)
			inputData[i] = i % 256;

		auto* pD = inputData;
		for (size_t i = 0; i < H; ++i, pD += 4)
			filler->Write(pD, 1, 4, i);

		REQUIRE(prg.nrCallsStart2 == 1);
		REQUIRE(prg.nrCallsProgress2 == 0); // AVX BitmapFiller doesn't make progress callbacks anymore.
		REQUIRE(prg.argumentStart2 == H);
	}
}
