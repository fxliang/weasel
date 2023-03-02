#include "stdafx.h"
#include "VerticalLayout.h"

using namespace weasel;

VerticalLayout::VerticalLayout(const UIStyle &style, const Context &context, const Status &status)
	: StandardLayout(style, context, status)
{
}

void weasel::VerticalLayout::DoLayout(CDCHandle dc, DirectWriteResources* pDWR)
{
	const std::vector<Text> &candidates(_context.cinfo.candies);
	const std::vector<Text> &comments(_context.cinfo.comments);
	const std::vector<Text> &labels(_context.cinfo.labels);
	CSize size;
	//const int space = size.cx / 4;
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
	/* Preedit */
	if (!IsInlinePreedit() && !_context.preedit.str.empty())
	{
		size = GetPreeditSize(dc, _context.preedit, pDWR->pTextFormat, pDWR);
		if(STATUS_ICON_SIZE/ 2 >= (height + size.cy / 2) && ShouldDisplayStatusIcon())
		{
			height += (STATUS_ICON_SIZE - size.cy) / 2;
			_preeditRect.SetRect(real_margin_x, height, real_margin_x + size.cx, height + size.cy);
			height += size.cy + (STATUS_ICON_SIZE - size.cy) / 2 + _style.spacing;
		}
		else
		{
			_preeditRect.SetRect(real_margin_x, height, real_margin_x + size.cx, height + size.cy);
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
			_auxiliaryRect.SetRect(real_margin_x, height, real_margin_x + size.cx, height + size.cy);
			height += size.cy + (STATUS_ICON_SIZE - size.cy) / 2 + _style.spacing;
		}
		else
		{
			_auxiliaryRect.SetRect(real_margin_x, height, real_margin_x + size.cx, height + size.cy);
			height += size.cy + _style.spacing;
		}
		_auxiliaryRect.OffsetRect(offsetX, offsetY);
		width = max(width, real_margin_x + size.cx + real_margin_x);
	}

	/* Candidates */
	int comment_shift_width = 0;  /* distance to the left of the candidate text */
	int max_candidate_width = 0;  /* label + text */
	int max_comment_width = 0;    /* comment, or none */
	for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
	{
		if (i > 0 )
			height += _style.candidate_spacing;

		int w = real_margin_x + base_offset, h = 0;
		int candidate_width = 0, comment_width = 0;
		int labelFontValid = !!(_style.label_font_point > 0);
		int textFontValid = !!(_style.font_point > 0);
		int cmtFontValid = !!(_style.comment_font_point > 0);
		/* Label */
		std::wstring label = GetLabelText(labels, i, _style.label_text_format.c_str());
		GetTextSizeDW(label, label.length(), pDWR->pLabelTextFormat, pDWR, &size);
		_candidateLabelRects[i].SetRect(w, height, w + size.cx * labelFontValid, height + size.cy);
		_candidateLabelRects[i].OffsetRect(offsetX, offsetY);
		w += (size.cx + space) * labelFontValid, h = max(h, size.cy);
		candidate_width += (size.cx + space) * labelFontValid;

		/* Text */
		const std::wstring& text = candidates.at(i).str;
		GetTextSizeDW(text, text.length(), pDWR->pTextFormat, pDWR, &size);
		_candidateTextRects[i].SetRect(w, height, w + size.cx * textFontValid, height + size.cy);
		_candidateTextRects[i].OffsetRect(offsetX, offsetY);
		w += size.cx * textFontValid, h = max(h, size.cy);
		candidate_width += size.cx * textFontValid;
		max_candidate_width = max(max_candidate_width, candidate_width);

		/* Comment */
		if (!comments.at(i).str.empty() && cmtFontValid)
		{
			w += space;
			comment_shift_width = max(comment_shift_width, w - real_margin_x);

			const std::wstring& comment = comments.at(i).str;
			GetTextSizeDW(comment, comment.length(), pDWR->pCommentTextFormat, pDWR, &size);
			_candidateCommentRects[i].SetRect(0, height, size.cx, height + size.cy);
			_candidateCommentRects[i].OffsetRect(offsetX, offsetY);
			w += size.cx, h = max(h, size.cy);
			comment_width += size.cx;
			max_comment_width = max(max_comment_width, comment_width);
		}
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
		//w += margin;
		//width = max(width, w);
		height += h - 1;
	}
	/* comments are left-aligned to the right of the longest candidate who has a comment */
	int max_content_width = max(max_candidate_width, comment_shift_width + max_comment_width);
	width = max(width, max_content_width + 2 * real_margin_x);
	width += base_offset;
	/* Align comments */
	for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
		_candidateCommentRects[i].OffsetRect(real_margin_x + comment_shift_width, 0);

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
	_contentSize.SetSize(width + offsetX * 2, height + offsetY * 2);

	/* Highlighted Candidate */
	int id = _context.cinfo.highlighted;
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

		_candidateRects[i].SetRect(real_margin_x + offsetX, hlTop, width - real_margin_x + offsetX, hlBot);
	}
	if (_style.hilited_mark_color & 0xff000000)
		width -= MARK_GAP;
	_highlightRect = _candidateRects[id];

	// calc roundings start
	_contentRect.SetRect(0, 0, _contentSize.cx, _contentSize.cy);
	CopyRect(_bgRect, _contentRect);
	_bgRect.DeflateRect(offsetX + 1, offsetY + 1);
	_PrepareRoundInfo(dc);

	int deflatex = offsetX - _style.border / 2;
	int deflatey = offsetY - _style.border / 2;
	_contentRect.DeflateRect(deflatex, deflatey);
	if (_style.border % 2 == 0)	_contentRect.DeflateRect(1, 1);
}
