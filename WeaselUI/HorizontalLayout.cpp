#include "stdafx.h"
#include "HorizontalLayout.h"

using namespace weasel;

HorizontalLayout::HorizontalLayout(const UIStyle &style, const Context &context, const Status &status)
	: StandardLayout(style, context, status)
{
}

void HorizontalLayout::DoLayout(CDCHandle dc, GDIFonts* pFonts, DirectWriteResources* pDWR )
{
	const std::vector<Text> &candidates(_context.cinfo.candies);
	const std::vector<Text> &comments(_context.cinfo.comments);
	const std::vector<Text> &labels(_context.cinfo.labels);
	int id = _context.cinfo.highlighted;
	CSize size;
	//dc.GetTextExtent(L"\x4e2d", 1, &size);
	//const int space = size.cx / 4;
	const int space = _style.hilite_spacing;
	int real_margin_x = (abs(_style.margin_x) > _style.hilite_padding) ? abs(_style.margin_x) : _style.hilite_padding;
	int real_margin_y = (abs(_style.margin_y) > _style.hilite_padding) ? abs(_style.margin_y) : _style.hilite_padding;
	int width = 0, height = real_margin_y;

	CFont labelFont, textFont, commentFont;
	CFontHandle oldFont;
	if (!_style.color_font)
	{
		long hlabel = -MulDiv(pFonts->m_LabelFont.m_FontPoint, dc.GetDeviceCaps(LOGPIXELSY), 72);
		long htext = -MulDiv(pFonts->m_TextFont.m_FontPoint, dc.GetDeviceCaps(LOGPIXELSY), 72);
		long hcmmt = -MulDiv(pFonts->m_CommentFont.m_FontPoint, dc.GetDeviceCaps(LOGPIXELSY), 72);
		labelFont.CreateFontW(hlabel, 0, 0, 0, pFonts->m_LabelFont.m_FontWeight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, pFonts->m_LabelFont.m_FontFace.c_str());
		textFont.CreateFontW(htext, 0, 0, 0, pFonts->m_TextFont.m_FontWeight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, pFonts->m_TextFont.m_FontFace.c_str());
		commentFont.CreateFontW(hcmmt, 0, 0, 0, pFonts->m_CommentFont.m_FontWeight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, pFonts->m_CommentFont.m_FontFace.c_str());
		oldFont = dc.SelectFont(textFont);
	}
	if (!_style.mark_text.empty() && (_style.hilited_mark_color & 0xff000000))
	{
		CSize sg;
		if (_style.color_font)
			GetTextSizeDW(_style.mark_text, _style.mark_text.length(), pDWR->pTextFormat, pDWR, &sg);
		else
			GetTextExtentDCMultiline(dc, _style.mark_text, _style.mark_text.length(), &sg);
		MARK_WIDTH = sg.cx;
		MARK_HEIGHT = sg.cy;
		MARK_GAP = MARK_WIDTH + 4;
	}
	int base_offset =  (_style.hilited_mark_color & 0xff000000) ? MARK_GAP : 0;
	/* Preedit */
	if (!IsInlinePreedit() && !_context.preedit.str.empty())
	{
		if (_style.color_font)
			size = GetPreeditSize(dc, _context.preedit, pDWR->pTextFormat, pDWR);
		else
			size = GetPreeditSize(dc, _context.preedit);
		if(STATUS_ICON_SIZE/ 2 >= (height + size.cy / 2) && ShouldDisplayStatusIcon())
		{
			height += (STATUS_ICON_SIZE - size.cy) / 2;
			_preeditRect.SetRect(real_margin_x , height, real_margin_x  + size.cx, height + size.cy);
			height += size.cy + (STATUS_ICON_SIZE - size.cy) / 2 + _style.spacing;
		}
		else
		{
			_preeditRect.SetRect(real_margin_x , height, real_margin_x  + size.cx, height + size.cy);
			height += size.cy + _style.spacing;
		}
		_preeditRect.OffsetRect(offsetX, offsetY);
		width = max(width, real_margin_x + size.cx + real_margin_x);
	}

	/* Auxiliary */
	if (!_context.aux.str.empty())
	{
		if (_style.color_font)
			size = GetPreeditSize(dc, _context.aux, pDWR->pTextFormat, pDWR);
		else
			size = GetPreeditSize(dc, _context.aux);
		if(STATUS_ICON_SIZE/ 2 >= (height + size.cy / 2) && ShouldDisplayStatusIcon())
		{
			height += (STATUS_ICON_SIZE - size.cy) / 2 ;
			_auxiliaryRect.SetRect(real_margin_x , height, real_margin_x  + size.cx, height + size.cy);
			height += size.cy + (STATUS_ICON_SIZE - size.cy) / 2 + _style.spacing;
		}
		else
		{
			_auxiliaryRect.SetRect(real_margin_x , height, real_margin_x  + size.cx, height + size.cy);
			height += size.cy + _style.spacing;
		}
		_auxiliaryRect.OffsetRect(offsetX, offsetY);
		width = max(width, real_margin_x + size.cx + real_margin_x);
	}

	/* Candidates */
	int w = real_margin_x, h = 0;
	for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
	{
		if (i == id)
			w += base_offset;
		if (i > 0)
			w += _style.candidate_spacing - 1;

		/* Label */
		std::wstring label = GetLabelText(labels, i, _style.label_text_format.c_str());
		if (_style.color_font)
			GetTextSizeDW(label, label.length(), pDWR->pLabelTextFormat, pDWR, &size);
		else
		{
			oldFont = dc.SelectFont(labelFont);
			GetTextExtentDCMultiline(dc, label, label.length(), &size);
		}

		_candidateLabelRects[i].SetRect(w, height, w + size.cx * ((int)(pFonts->m_LabelFont.m_FontPoint > 0)), height + size.cy);
		_candidateLabelRects[i].OffsetRect(offsetX, offsetY);
		w += (size.cx + space) * ((int)(pFonts->m_LabelFont.m_FontPoint > 0)), h = max(h, size.cy);
		//w += space;

		/* Text */
		const std::wstring& text = candidates.at(i).str;
		if (_style.color_font)
			GetTextSizeDW(text, text.length(), pDWR->pTextFormat, pDWR, &size);
		else
		{
			oldFont = dc.SelectFont(textFont);
			GetTextExtentDCMultiline(dc, text, text.length(), &size);
		}

		_candidateTextRects[i].SetRect(w, height, w + size.cx * ((int)(pFonts->m_TextFont.m_FontPoint > 0)), height + size.cy);
		_candidateTextRects[i].OffsetRect(offsetX, offsetY);
		w += (size.cx + space) * ((int)(pFonts->m_TextFont.m_FontPoint > 0)), h = max(h, size.cy);

		/* Comment */
		if (!comments.at(i).str.empty() && pFonts->m_CommentFont.m_FontPoint > 0)
		{
			const std::wstring& comment = comments.at(i).str;
			if (_style.color_font)
				GetTextSizeDW(comment, comment.length(), pDWR->pCommentTextFormat, pDWR, &size);
			else
			{
				oldFont = dc.SelectFont(commentFont);
				GetTextExtentDCMultiline(dc, comment, comment.length(), &size);
			}

			_candidateCommentRects[i].SetRect(w, height, w + size.cx + space, height + size.cy);
			w += (size.cx + space) * ((int)(pFonts->m_CommentFont.m_FontPoint > 0)), h = max(h, size.cy);
		}
		else /* Used for highlighted candidate calculation below */
		{
			_candidateCommentRects[i].SetRect(w, height, w, height + size.cy);
		}
		_candidateCommentRects[i].OffsetRect(offsetX, offsetY);
	}
	if(!_style.color_font)
		dc.SelectFont(oldFont);
	int newh = 0;
	int mintop = _candidateTextRects[0].bottom;
	int maxbot = _candidateTextRects[0].top;
	for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
	{
		int ol = 0, ot = 0, oc = 0;
		if (_style.align_type == UIStyle::ALIGN_CENTER)
		{
			ol = (h - _candidateLabelRects[i].Height()) / 2;
			ot = (h - _candidateTextRects[i].Height()) / 2;
			oc = (h - _candidateCommentRects[i].Height()) / 2;
		}
		else if (_style.align_type == UIStyle::ALIGN_BOTTOM)
		{
			ol = (h - _candidateLabelRects[i].Height()) ;
			ot = (h - _candidateTextRects[i].Height()) ;
			oc = (h - _candidateCommentRects[i].Height()) ;

		}
		_candidateLabelRects[i].OffsetRect(0, ol);
		_candidateTextRects[i].OffsetRect(0, ot);
		_candidateCommentRects[i].OffsetRect(0, oc);
		mintop = min(mintop, _candidateLabelRects[i].top);
		mintop = min(mintop, _candidateTextRects[i].top);
		mintop = min(mintop, _candidateCommentRects[i].top);
		maxbot = max(maxbot, _candidateLabelRects[i].bottom);
		maxbot = max(maxbot, _candidateTextRects[i].bottom);
		maxbot = max(maxbot, _candidateCommentRects[i].bottom);
		newh = min(newh, ol);
		newh = min(newh, ot);
		newh = min(newh, oc);
	}
	h -= newh;
	w += real_margin_x;

	/* Highlighted Candidate */
	for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
	{
		int hlTop = _candidateTextRects[i].top;
		int hlBot = _candidateTextRects[i].bottom;

		if (_candidateLabelRects[i].Height() > 0)
		{
			hlTop = min(_candidateLabelRects[i].top, hlTop);
			hlBot = max(_candidateLabelRects[i].bottom, _candidateTextRects[i].bottom);
		}
		if (_candidateCommentRects[i].Height() > 0)
		{
			hlTop = min(hlTop, _candidateCommentRects[i].top);
			hlBot = max(hlBot, _candidateCommentRects[i].bottom);
		}
		hlTop = min(mintop, hlTop);
		hlBot = max(maxbot, hlBot);
		int gap = (_style.hilited_mark_color & 0xff000000)!=0 && id == i ? base_offset : 0;
		_candidateRects[i].SetRect(_candidateLabelRects[i].left - gap, hlTop, _candidateCommentRects[i].right, hlBot);
	}

	width = max(width, w);
	height += h;

	if (candidates.size())
		height += _style.spacing;

	/* Trim the last spacing */
	if (height > 0)
		height -= _style.spacing;
	height += real_margin_y;

	if (!_context.preedit.str.empty() && !candidates.empty())
	{
		width = max(width, _style.min_width);
		height = max(height, _style.min_height);
	}
	UpdateStatusIconLayout(&width, &height);
	_contentSize.SetSize(width + 2 * offsetX, height + 2 * offsetY);

	_candidateRects[candidates.size() - 1].right = width - real_margin_x + offsetX;
	_highlightRect = _candidateRects[id];

	labelFont.DeleteObject();
	textFont.DeleteObject();
	commentFont.DeleteObject();
	oldFont.DeleteObject();
}
