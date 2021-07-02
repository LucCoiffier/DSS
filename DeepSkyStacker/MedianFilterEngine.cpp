#include <stdafx.h>
#include <omp.h>
#include "MedianFilterEngine.h"
#include "avx_filter.h"


template <typename T>
void CInternalMedianFilterEngineT<T>::CFilterTask::process()
{
	const int height = m_pEngine->m_lHeight;
	const int nrProcessors = CMultitask::GetNrProcessors();
	constexpr int lineBlockSize = 20;
	const size_t filterSize = m_pEngine->m_lFilterSize;
	int progress = 0;

	if (m_pProgress != nullptr)
		m_pProgress->SetNrUsedProcessors(nrProcessors);

	AvxImageFilter avxFilter(m_pEngine);
	std::vector<T> values((filterSize * 2 + 1) * (filterSize * 2 + 1));

#pragma omp parallel for default(none) firstprivate(avxFilter, values) schedule(guided, 5) if(nrProcessors > 1)
	for (int row = 0; row < height; row += lineBlockSize)
	{
		if (omp_get_thread_num() == 0 && m_pProgress != nullptr)
			m_pProgress->Progress2(nullptr, progress += nrProcessors * lineBlockSize);

		const int endRow = std::min(row + lineBlockSize, height);
		if (avxFilter.filter(row, endRow) != 0)
		{
			this->processNonAvx(row, endRow, values);
		}
	}

	if (m_pProgress != nullptr)
		m_pProgress->SetNrUsedProcessors();
}

template <typename T>
void CInternalMedianFilterEngineT<T>::CFilterTask::processNonAvx(const int lineStart, const int lineEnd, std::vector<T>& vValues)
{
	{
		const CFATYPE CFAType = m_pEngine->m_CFAType;
		const int width = m_pEngine->m_lWidth;
		const int height = m_pEngine->m_lHeight;
		const int filterSize = m_pEngine->m_lFilterSize;

		if (CFAType != CFATYPE_NONE)
		{
			T* pOutValues = m_pEngine->m_pvOutValues + lineStart * static_cast<size_t>(width);

			for (int row = lineStart; row < lineEnd; ++row)
			{
				for (int col = 0; col < width; ++col)
				{
					// Compute the min and max values in X and Y
					const int xMin = std::max(0, col - filterSize);
					const int xMax = std::min(col + filterSize, width - 1);
					const int yMin = std::max(0, row - filterSize);
					const int yMax = std::min(row + filterSize, height - 1);

					// Fill the array with the values
					const T* pInLine = m_pEngine->m_pvInValues + xMin + yMin * static_cast<size_t>(width);
					const BAYERCOLOR BayerColor = GetBayerColor(col, row, CFAType);
					vValues.resize(0);
					for (int k = yMin; k <= yMax; k++)
					{
						const T* pInValues = pInLine;
						for (int l = xMin; l <= xMax; l++, pInValues++)
						{
							if (GetBayerColor(l, k, CFAType) == BayerColor)
								vValues.push_back(*pInValues);
						}
						pInLine += width;
					}

					*pOutValues = static_cast<T>(Median(vValues));
					pOutValues++;
				}
			}
		}
		else
		{
			T* pOutValues = m_pEngine->m_pvOutValues + lineStart * static_cast<size_t>(width);

			for (int row = lineStart; row < lineEnd; ++row)
			{
				for (int col = 0; col < width; ++col)
				{
					// Compute the min and max values in X and Y
					const int xMin = std::max(0, col - filterSize);
					const int xMax = std::min(col + filterSize, width - 1);
					const int yMin = std::max(0, row - filterSize);
					const int yMax = std::min(row + filterSize, height - 1);
					const size_t xRange = xMax - xMin + size_t{ 1 };

					vValues.resize(xRange * (yMax - yMin + 1));

					// Fill the array with the values
					const T* pInValues = m_pEngine->m_pvInValues + xMin + yMin * static_cast<size_t>(width);
					T* pAreaValues = vValues.data();
					for (int k = yMin; k <= yMax; k++, pInValues += width, pAreaValues += xRange)
					{
						memcpy(pAreaValues, pInValues, sizeof(T) * xRange);
					}

					*pOutValues = static_cast<T>(Median(vValues));
					pOutValues++;
				}
			}
		}
	}
}

template <typename TType>
bool CInternalMedianFilterEngineT<TType>::ApplyFilter(CDSSProgress* pProgress)
{
	bool bResult = true;

	if (pProgress != nullptr)
		pProgress->Start2(nullptr, m_lHeight);

	CFilterTask{ this, pProgress }.process();

	if (pProgress != nullptr)
		pProgress->End2();

	return bResult;
}


template bool CInternalMedianFilterEngineT<unsigned char>::ApplyFilter(CDSSProgress*);
template bool CInternalMedianFilterEngineT<unsigned short>::ApplyFilter(CDSSProgress*);
template bool CInternalMedianFilterEngineT<unsigned int>::ApplyFilter(CDSSProgress*);
template bool CInternalMedianFilterEngineT<float>::ApplyFilter(CDSSProgress*);
template bool CInternalMedianFilterEngineT<double>::ApplyFilter(CDSSProgress*);