#include "stdafx.h"
#include "catch.h"
#include "../DeepSkyStacker/avx_cfa.h"

TEST_CASE("AVX CFA", "[AVX][CFA]")
{
	const auto compare = [](const WORD* pColor, const std::initializer_list<WORD>& data)
	{
		return memcmp(pColor, data.begin(), data.size() * sizeof(WORD));
	};

	CSmartPtr<CMemoryBitmap> pBitmap;
	pBitmap.Attach(new CGrayBitmapT<WORD>);
	AvxCfaProcessing avxCfaProcessing(0, 0, *pBitmap);

	SECTION("Init initializes the object and sets nrVectorsPerLine properly")
	{
		const bool b = pBitmap->Init(8, 1);
		REQUIRE(b == true);
		avxCfaProcessing.init(0, 1);
		REQUIRE(avxCfaProcessing.nrVectorsPerLine() == 1);
	}

	SECTION("Init sets nrVectorsPerLine to 2 for width 17")
	{
		const bool b = pBitmap->Init(17, 1);
		REQUIRE(b == true);
		avxCfaProcessing.init(0, 1);
		REQUIRE(avxCfaProcessing.nrVectorsPerLine() == 2);
	}

	SECTION("Interpolate returns 1 if pixelsizemultiplier is 2")
	{
		avxCfaProcessing.init(0, 1);
		REQUIRE(avxCfaProcessing.interpolate(0, 1, 2) == 1);
	}

	SECTION("Interpolate fails if CFA type is NONE")
	{
		pBitmap->Init(64, 8);
		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		REQUIRE(pGray != nullptr);
		pGray->UseBilinear(true);
		pGray->SetCFAType(CFATYPE_NONE);
		memset(pGray->m_vPixels.data(), 0, sizeof(WORD) * 64 * 8);
		avxCfaProcessing.init(0, 8);
		REQUIRE(avxCfaProcessing.interpolate(0, 1, 1) != 0);
	}

	SECTION("Interpolate fails if CFA type is BGGR")
	{
		pBitmap->Init(64, 8);
		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		REQUIRE(pGray != nullptr);
		pGray->UseBilinear(true);
		pGray->SetCFAType(CFATYPE_BGGR);
		memset(pGray->m_vPixels.data(), 0, sizeof(WORD) * 64 * 8);
		avxCfaProcessing.init(0, 8);
		REQUIRE(avxCfaProcessing.interpolate(0, 1, 1) != 0);
	}

	SECTION("Interpolate fails if CFA type is GRBG")
	{
		pBitmap->Init(64, 8);
		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		REQUIRE(pGray != nullptr);
		pGray->UseBilinear(true);
		pGray->SetCFAType(CFATYPE_GRBG);
		memset(pGray->m_vPixels.data(), 0, sizeof(WORD) * 64 * 8);
		avxCfaProcessing.init(0, 8);
		REQUIRE(avxCfaProcessing.interpolate(0, 1, 1) != 0);
	}

	SECTION("Interpolate fails if CFA type is GBRG")
	{
		pBitmap->Init(64, 8);
		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		REQUIRE(pGray != nullptr);
		pGray->UseBilinear(true);
		pGray->SetCFAType(CFATYPE_GBRG);
		memset(pGray->m_vPixels.data(), 0, sizeof(WORD) * 64 * 8);
		avxCfaProcessing.init(0, 8);
		REQUIRE(avxCfaProcessing.interpolate(0, 1, 1) != 0);
	}

	SECTION("Interpolate fails if CFA transform is superpixel although CFA type is RGGB")
	{
		pBitmap->Init(64, 8);
		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		REQUIRE(pGray != nullptr);
		pGray->UseSuperPixels(true);
		pGray->SetCFAType(CFATYPE_RGGB);
		memset(pGray->m_vPixels.data(), 0, sizeof(WORD) * 64 * 8);
		avxCfaProcessing.init(0, 8);
		REQUIRE(avxCfaProcessing.interpolate(0, 1, 1) != 0);
	}

	SECTION("Interpolate fails if CFA transform is rawbayer although CFA type is RGGB")
	{
		pBitmap->Init(64, 8);
		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		REQUIRE(pGray != nullptr);
		pGray->UseRawBayer(true);
		pGray->SetCFAType(CFATYPE_RGGB);
		memset(pGray->m_vPixels.data(), 0, sizeof(WORD) * 64 * 8);
		avxCfaProcessing.init(0, 8);
		REQUIRE(avxCfaProcessing.interpolate(0, 1, 1) != 0);
	}

	SECTION("Interpolate fails if CFA transform is AHD although CFA type is RGGB")
	{
		pBitmap->Init(64, 8);
		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		REQUIRE(pGray != nullptr);
		pGray->UseAHD(true);
		pGray->SetCFAType(CFATYPE_RGGB);
		memset(pGray->m_vPixels.data(), 0, sizeof(WORD) * 64 * 8);
		avxCfaProcessing.init(0, 8);
		REQUIRE(avxCfaProcessing.interpolate(0, 1, 1) != 0);
	}

	SECTION("Interpolate zeros gives zeros")
	{
		const bool b = pBitmap->Init(64, 8);
		REQUIRE(b == true);
		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		pGray->UseBilinear(true);
		pGray->SetCFAType(CFATYPE_RGGB);
		memset(pGray->m_vPixels.data(), 0, sizeof(WORD) * 64 * 8);
		
		avxCfaProcessing.init(0, 8);
		const int r = avxCfaProcessing.interpolate(0, 8, 1);
		REQUIRE(r == 0);

		WORD d[64] = { 0 };
		REQUIRE(memcmp(avxCfaProcessing.redCfaLine<WORD>(0), d, 128) == 0);
		REQUIRE(memcmp(avxCfaProcessing.greenCfaLine<WORD>(0), d, 128) == 0);
		REQUIRE(memcmp(avxCfaProcessing.blueCfaLine<WORD>(0), d, 128) == 0);
	}

	SECTION("Interpolate a RGRG line")
	{
		REQUIRE(pBitmap->Init(64, 8) == true);
		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		pGray->UseBilinear(true);
		pGray->SetCFAType(CFATYPE_RGGB);
		WORD v = 0;
		for (WORD& x : pGray->m_vPixels) {
			x = v++;
		}

		avxCfaProcessing.init(0, 8);
		REQUIRE(avxCfaProcessing.interpolate(0, 8, 1) == 0);

		REQUIRE(compare(avxCfaProcessing.redCfaLine<WORD>(2), { 128, 129, 130, 131, 132, 133 }) == 0);
		REQUIRE(compare(avxCfaProcessing.greenCfaLine<WORD>(2), { 65, 129, 130, 131, 132, 133 }) == 0);
		REQUIRE(compare(avxCfaProcessing.blueCfaLine<WORD>(2), { 65, 129, 130, 131, 132, 133 }) == 0);
	}

	SECTION("Interpolate a GBGB line")
	{
		REQUIRE(pBitmap->Init(64, 8) == true);
		auto* pGray = dynamic_cast<CGrayBitmapT<WORD>*>(pBitmap.m_p);
		pGray->UseBilinear(true);
		pGray->SetCFAType(CFATYPE_RGGB);
		WORD v = 0;
		for (WORD& x : pGray->m_vPixels) {
			x = v++;
		}

		avxCfaProcessing.init(0, 8);
		REQUIRE(avxCfaProcessing.interpolate(0, 8, 1) == 0);

		REQUIRE(compare(avxCfaProcessing.redCfaLine<WORD>(3), { 192, 193, 194, 195, 196, 197 }) == 0);
		REQUIRE(compare(avxCfaProcessing.greenCfaLine<WORD>(3), { 192, 193, 194, 195, 196, 197 }) == 0);
		REQUIRE(compare(avxCfaProcessing.blueCfaLine<WORD>(3), { 97, 193, 194, 195, 196, 197 }) == 0);
	}
}




bool CMultitask::GetUseSimd() { return true; }
void CMultitask::CloseAllThreads() {}
void CMultitask::StartThreads(long) {}
unsigned long CMultitask::GetAvailableThreadId() { return 0; }

void CMultiBitmap::DestroyTempFiles() {}
void CMultiBitmap::SetBitmapModel(CMemoryBitmap*) {}
bool CMultiBitmap::AddBitmap(CMemoryBitmap*, CDSSProgress*) { return true; }
bool CMultiBitmap::GetResult(CMemoryBitmap**, CDSSProgress*) { return true; }

void CYMGToRGB(double, double, double, double, double&, double&, double&) {}
bool CreateBitmap(class CBitmapCharacteristics const&, class CMemoryBitmap**) { return true; }
bool CGrayMedianFilterEngineT<unsigned short>::GetFilteredImage(class CMemoryBitmap**, long, class CDSSProgress*) { return true; }

bool CColorMedianFilterEngineT<unsigned short>::GetFilteredImage(class CMemoryBitmap**, long, class CDSSProgress*) { return true; }
