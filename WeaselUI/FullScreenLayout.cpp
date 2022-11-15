#include "stdafx.h"
#include "FullScreenLayout.h"

using namespace weasel;

FullScreenLayout::FullScreenLayout(const UIStyle &style, const Context &context, const Status &status, const CRect& inputPos, Layout* layout)
	: StandardLayout(style, context, status), mr_inputPos(inputPos), m_layout(layout)
{
}

FullScreenLayout::~FullScreenLayout()
{
	delete m_layout;
}

void weasel::FullScreenLayout::DoLayout(CDCHandle dc, GDIFonts* pFonts, DirectWriteResources* pDWR)
{
	if (_context.empty())
	{
		int width = 0, height = 0;
		UpdateStatusIconLayout(&width, &height);
		_contentSize.SetSize(width, height);
		return;
	}

	CRect workArea;
	HMONITOR hMonitor = MonitorFromRect(mr_inputPos, MONITOR_DEFAULTTONEAREST);
	if (hMonitor)
	{
		MONITORINFO info;
		info.cbSize = sizeof(MONITORINFO);
		if (GetMonitorInfo(hMonitor, &info))
		{
			workArea = info.rcWork;
		}
	}

	int step = 256;
	do {
		if(_style.color_font)
			m_layout->DoLayout(dc, pFonts, pDWR);
		else
			m_layout->DoLayout(dc, pFonts);
		if (!_style.mark_text.empty())
		{
			CSize sg;
			if (_style.color_font)
				GetTextSizeDW(_style.mark_text, _style.mark_text.length(), pDWR->pTextFormat, pDWR->pDWFactory, &sg);
			else
				GetTextExtentDCMultiline(dc, _style.mark_text, _style.mark_text.length(), &sg);
			MARK_WIDTH = sg.cx;
			MARK_GAP = MARK_WIDTH + 4;
		}
	}
	while (AdjustFontPoint(dc, workArea, pFonts, step, pDWR));

	int offsetX = (workArea.Width() - m_layout->GetContentSize().cx) / 2;
	int offsetY = (workArea.Height() - m_layout->GetContentSize().cy) / 2;
	_preeditRect = m_layout->GetPreeditRect();
	_preeditRect.OffsetRect(offsetX, offsetY);
	_auxiliaryRect = m_layout->GetAuxiliaryRect();
	_auxiliaryRect.OffsetRect(offsetX, offsetY);
	_highlightRect = m_layout->GetHighlightRect();
	_highlightRect.OffsetRect(offsetX, offsetY);
	for (int i = 0, n = (int)_context.cinfo.candies.size(); i < n && i < MAX_CANDIDATES_COUNT; ++i)
	{
		_candidateLabelRects[i] = m_layout->GetCandidateLabelRect(i);
		_candidateLabelRects[i].OffsetRect(offsetX, offsetY);
		_candidateTextRects[i] = m_layout->GetCandidateTextRect(i);
		_candidateTextRects[i].OffsetRect(offsetX, offsetY);
		_candidateCommentRects[i] = m_layout->GetCandidateCommentRect(i);
		_candidateCommentRects[i].OffsetRect(offsetX, offsetY);
		_candidateRects[i] = m_layout->GetCandidateRect(i);
		_candidateRects[i].OffsetRect(offsetX, offsetY);
	}
	_statusIconRect = m_layout->GetStatusIconRect();

	_contentSize.SetSize(workArea.Width(), workArea.Height());
}

bool FullScreenLayout::AdjustFontPoint(CDCHandle dc, const CRect& workArea, GDIFonts* pFonts, int& step, DirectWriteResources* pDWR)
{
	if (_context.empty() || step == 0)
		return false;
	if (_style.color_font)
	{
		int fontPointLabel;
		int fontPoint;
		int fontPointComment;

		if (pDWR->pLabelTextFormat != NULL)
			fontPointLabel = pDWR->pLabelTextFormat->GetFontSize() / pDWR->dpiScaleX_;
		else
			fontPointLabel = 0;
		if (pDWR->pTextFormat != NULL)
			fontPoint = pDWR->pTextFormat->GetFontSize() / pDWR->dpiScaleX_;
		else
			fontPoint = 0;
		if (pDWR->pCommentTextFormat != NULL)
			fontPointComment = pDWR->pCommentTextFormat->GetFontSize() / pDWR->dpiScaleX_;
		else
			fontPointComment = 0;
		CSize sz = m_layout->GetContentSize();
		if (sz.cx > workArea.Width() - offsetX * 2 || sz.cy > workArea.Height() - offsetY * 2)
		{
			if (step > 0)
			{
				step = -(step >> 1);
			}
			fontPoint += step;
			fontPointLabel += step;
			fontPointComment += step;
			pDWR->InitResources(_style.label_font_face, fontPointLabel, _style.font_face, fontPoint, _style.comment_font_face, fontPointComment);
			return true;
		}
		else if (sz.cx <= (workArea.Width() - offsetX * 2) * 31 / 32 && sz.cy <= (workArea.Height() - offsetY * 2) * 31 / 32)
		{
			if (step < 0)
			{
				step = -step >> 1;
			}
			fontPoint += step;
			fontPointLabel += step;
			fontPointComment += step;
			pDWR->InitResources(_style.label_font_face, fontPointLabel, _style.font_face, fontPoint, _style.comment_font_face, fontPointComment);
			return true;
		}

		return false;
	}
	else
	{

		CSize sz = m_layout->GetContentSize();
		if (sz.cx > workArea.Width() - offsetX * 2 || sz.cy > workArea.Height() - offsetY * 2)
		{
			if (step > 0)
			{
				step = -(step >> 1);
			}
			pFonts->m_LabelFont.m_FontPoint += step;
			pFonts->m_TextFont.m_FontPoint += step;
			pFonts->m_CommentFont.m_FontPoint += step;
			return true;
		}
		else if (sz.cx <= (workArea.Width() - offsetX * 2) * 31 / 32 && sz.cy <= (workArea.Height() - offsetY * 2) * 31 / 32)
		{
			if (step < 0)
			{
				step = -step >> 1;
			}
			pFonts->m_LabelFont.m_FontPoint += step;
			pFonts->m_TextFont.m_FontPoint += step;
			pFonts->m_CommentFont.m_FontPoint += step;
			return true;
		}

		return false;
	}
}
