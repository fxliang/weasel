
#include "stdafx.h"
#include "VHorizontalLayout.h"

using namespace weasel;

VHorizontalLayout::VHorizontalLayout(const UIStyle &style, const Context &context, const Status &status)
	: StandardLayout(style, context, status)
{
}

void VHorizontalLayout::DoLayout(CDCHandle dc, DirectWriteResources* pDWR )
{
	const std::vector<Text> &candidates(_context.cinfo.candies);
	const std::vector<Text> &comments(_context.cinfo.comments);
	const std::vector<Text> &labels(_context.cinfo.labels);
	int id = _context.cinfo.highlighted;
	CSize size;

	const int space = _style.hilite_spacing;
	int real_margin_x = (abs(_style.margin_x) > _style.hilite_padding) ? abs(_style.margin_x) : _style.hilite_padding;
	int real_margin_y = (abs(_style.margin_y) > _style.hilite_padding) ? abs(_style.margin_y) : _style.hilite_padding;
	int width = 0, height = real_margin_y;

	if (!_style.mark_text.empty() && (_style.hilited_mark_color & 0xff000000))
	{
		CSize sg;
		GetTextSizeDW(_style.mark_text, _style.mark_text.length(), pDWR->pTextFormat, pDWR, &sg);
		MARK_WIDTH = sg.cx;
		MARK_HEIGHT = sg.cy;
		MARK_GAP = MARK_WIDTH + 4;
	}
	int base_offset =  (_style.hilited_mark_color & 0xff000000) ? MARK_GAP : 0;
	int labelFontValid = !!(_style.label_font_point > 0);
	int textFontValid = !!(_style.font_point > 0);
	int cmtFontValid = !!(_style.comment_font_point > 0);
	/* Preedit */
	if (!IsInlinePreedit() && !_context.preedit.str.empty())
	{
		size = GetPreeditSize(dc, _context.preedit, pDWR->pTextFormat, pDWR);
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
		size = GetPreeditSize(dc, _context.aux, pDWR->pTextFormat, pDWR);
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
	int w = real_margin_x, h = height, ww = 0;
	for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
	{
		h = real_margin_y;
		//if (i == id) w += base_offset;
		if (i > 0)
			w += ww + _style.candidate_spacing - 1;

		/* Label */
		std::wstring label = GetLabelText(labels, i, _style.label_text_format.c_str());
		GetTextSizeDW(label, label.length(), pDWR->pLabelTextFormat, pDWR, &size);

		_candidateLabelRects[i].SetRect(w, h, w + size.cx * labelFontValid, h + size.cy);
		_candidateLabelRects[i].OffsetRect(offsetX, offsetY);
		//w += (size.cx + space) * ((int)(pFonts->m_LabelFont.m_FontPoint > 0)), h = max(h, size.cy);
		h += (size.cy + space) * labelFontValid;
		ww = max(ww, size.cx);
		height = max(height, h);

		/* Text */
		const std::wstring& text = candidates.at(i).str;
		GetTextSizeDW(text, text.length(), pDWR->pTextFormat, pDWR, &size);

		_candidateTextRects[i].SetRect(w, h, w + size.cx * textFontValid, h + size.cy);
		_candidateTextRects[i].OffsetRect(offsetX, offsetY);
		//w += (size.cx + space) * ((int)(pFonts->m_TextFont.m_FontPoint > 0)), h = max(h, size.cy);
		h += (size.cy + space) * textFontValid;
		ww = max(ww, size.cx);
		height = max(height, h);

		/* Comment */
		if (!comments.at(i).str.empty() && cmtFontValid)
		{
			const std::wstring& comment = comments.at(i).str;
			GetTextSizeDW(comment, comment.length(), pDWR->pCommentTextFormat, pDWR, &size);

			_candidateCommentRects[i].SetRect(w, h, w + size.cx + space, h + size.cy);
			//w += (size.cx + space) * ((int)(pFonts->m_CommentFont.m_FontPoint > 0)), h = max(h, size.cy);
			h += (size.cy + space) * cmtFontValid;
			ww = max(ww, size.cx);
			height = max(height, h);
		}
		else /* Used for highlighted candidate calculation below */
		{
			_candidateCommentRects[i].SetRect(w, h, w, h + size.cy);
		}
		_candidateCommentRects[i].OffsetRect(offsetX, offsetY);
	}

#if 1
	int neww = 0;
	int mintop = _candidateTextRects[0].bottom;
	int maxbot = _candidateTextRects[0].top;
	for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
	{
		//int ol = 0, ot = 0, oc = 0;
		//if (_style.align_type == UIStyle::ALIGN_CENTER)
		//{
		//	ol = (h - _candidateLabelRects[i].Height()) / 2;
		//	ot = (h - _candidateTextRects[i].Height()) / 2;
		//	oc = (h - _candidateCommentRects[i].Height()) / 2;
		//}
		//else if (_style.align_type == UIStyle::ALIGN_BOTTOM)
		//{
		//	ol = (h - _candidateLabelRects[i].Height()) ;
		//	ot = (h - _candidateTextRects[i].Height()) ;
		//	oc = (h - _candidateCommentRects[i].Height()) ;

		//}
		//_candidateLabelRects[i].OffsetRect(0, ol);
		//_candidateTextRects[i].OffsetRect(0, ot);
		//_candidateCommentRects[i].OffsetRect(0, oc);

		mintop = min(mintop, _candidateLabelRects[i].top);
		mintop = min(mintop, _candidateTextRects[i].top);
		mintop = min(mintop, _candidateCommentRects[i].top);
		maxbot = max(maxbot, _candidateLabelRects[i].bottom);
		maxbot = max(maxbot, _candidateTextRects[i].bottom);
		maxbot = max(maxbot, _candidateCommentRects[i].bottom);
		//neww = min(neww, ol);
		//neww = min(neww, ot);
		//neww = min(neww, oc);
	}
	//w -= neww;
#endif
	w += real_margin_x;

	/* Highlighted Candidate */
	for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
	{
		int hlLeft = _candidateTextRects[i].left;
		int hlRight = _candidateTextRects[i].right;

		if (_candidateLabelRects[i].Width() > 0)
		{
			hlLeft = min(_candidateLabelRects[i].left, hlLeft);
			hlRight = max(_candidateLabelRects[i].right, hlRight);
		}
		if (_candidateCommentRects[i].Height() > 0)
		{
			hlLeft = min(hlLeft, _candidateCommentRects[i].left);
			hlRight = max(hlRight, _candidateCommentRects[i].right);
		}
		//hlTop = min(mintop, hlTop);
		//hlBot = max(maxbot, hlBot);
		int gap = (_style.hilited_mark_color & 0xff000000)!=0 && id == i ? base_offset : 0;
		int hlTop = min(_candidateLabelRects[i].top, mintop);
		int hlBot = max(_candidateCommentRects[i].bottom, maxbot);
		//_candidateRects[i].SetRect(_candidateLabelRects[i].left - gap, hlTop, _candidateCommentRects[i].right, hlBot);
		_candidateRects[i].SetRect(hlLeft, mintop, hlRight, maxbot);
	}

	width = max(width, w);
	width += ww;
	//height += h;

	if (candidates.size())
		height += _style.spacing;

	/* Trim the last spacing */
	//if (height > 0) height -= _style.spacing;
	height += real_margin_y;

	if (!_context.preedit.str.empty() && !candidates.empty())
	{
		width = max(width, _style.min_width);
		height = max(height, _style.min_height);
	}
	UpdateStatusIconLayout(&width, &height);
	_contentSize.SetSize(width + 2 * offsetX, height + 2 * offsetY);

	_contentRect.SetRect(0, 0, _contentSize.cx, _contentSize.cy);
	CopyRect(_bgRect, _contentRect);
	_bgRect.DeflateRect(offsetX + 1, offsetY + 1);
	int deflatex = offsetX - _style.border / 2;
	int deflatey = offsetY - _style.border / 2;
	_contentRect.DeflateRect(deflatex, deflatey);
	if (_style.border % 2 == 0)	_contentRect.DeflateRect(1, 1);

	//_candidateRects[candidates.size() - 1].right = width - real_margin_x + offsetX;
	_highlightRect = _candidateRects[id];

}

