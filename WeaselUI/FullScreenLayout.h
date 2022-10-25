#pragma once

#include "StandardLayout.h"

namespace weasel
{
	class FullScreenLayout: public StandardLayout
	{
	public:
		FullScreenLayout(const UIStyle &style, const Context &context, const Status &status, const CRect& inputPos, Layout* layout);
		virtual ~FullScreenLayout();

		virtual void DoLayout(CDCHandle dc, GDIFonts* pFonts = NULL, DirectWriteResources* pDWR = NULL);

	private:
		bool AdjustFontPoint(CDCHandle dc, const CRect& workArea, GDIFonts* pFonts, int& step, DirectWriteResources* pDWR = NULL);

		const CRect& mr_inputPos;
		Layout* m_layout;
	};
};
