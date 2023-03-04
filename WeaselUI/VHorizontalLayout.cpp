
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
	int width = real_margin_x, height = real_margin_y;

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
		if(STATUS_ICON_SIZE/ 2 >= (width + size.cx / 2) && ShouldDisplayStatusIcon())
		{
			width += (STATUS_ICON_SIZE - size.cx) / 2;
			_preeditRect.SetRect(width , height, width  + size.cx, height + size.cy);
			width += size.cx + (STATUS_ICON_SIZE - size.cx) / 2 + _style.spacing;
		}
		else
		{
			_preeditRect.SetRect(real_margin_x , height, real_margin_x  + size.cx, height + size.cy);
			width += size.cx + _style.spacing;
		}
		_preeditRect.OffsetRect(offsetX, offsetY);
		//width = max(width, real_margin_x + size.cx + real_margin_x);
		height = max(height, real_margin_y + size.cy + real_margin_y);
	}

	/* Auxiliary */
	if (!_context.aux.str.empty())
	{
		size = GetPreeditSize(dc, _context.aux, pDWR->pTextFormat, pDWR);
		if(STATUS_ICON_SIZE/ 2 >= (width + size.cx / 2) && ShouldDisplayStatusIcon())
		{
			width += (STATUS_ICON_SIZE - size.cx) / 2;
			_auxiliaryRect.SetRect(real_margin_x , height, real_margin_x  + size.cx, height + size.cy);
			width += size.cx + (STATUS_ICON_SIZE - size.cx) / 2 + _style.spacing;
		}
		else
		{
			_auxiliaryRect.SetRect(real_margin_x , height, real_margin_x  + size.cx, height + size.cy);
			width += size.cx + _style.spacing;
		}
		_auxiliaryRect.OffsetRect(offsetX, offsetY);
		height = max(height, real_margin_y + size.cy + real_margin_y);
	}
	/* Candidates */
	int w = width, h = real_margin_y, hh = 0, max_bot_candidate_rect = 0;
	if (candidates.size() == 0)
		w -= _style.spacing;
	else
	{
		for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
		{
			int wid = 0;
			h = real_margin_y;
			/* Label */
			std::wstring label = GetLabelText(labels, i, _style.label_text_format.c_str());
			GetTextSizeDW(label, label.length(), pDWR->pLabelTextFormat, pDWR, &size);
			_candidateLabelRects[i].SetRect(w, h, w + size.cx * labelFontValid, h + size.cy);
			_candidateLabelRects[i].OffsetRect(offsetX, offsetY);
			h += (size.cy + space) * labelFontValid;
			height = max(height, h);
			wid = max(wid, size.cx);

			/* Text */
			const std::wstring& text = candidates.at(i).str;
			GetTextSizeDW(text, text.length(), pDWR->pTextFormat, pDWR, &size);
			_candidateTextRects[i].SetRect(w, h, w + size.cx * textFontValid, h + size.cy);
			_candidateTextRects[i].OffsetRect(offsetX, offsetY);
			h += (size.cy + space) * textFontValid;
			height = max(height, h);
			wid = max(wid, size.cx);

			/* Comment */
			if (!comments.at(i).str.empty() && cmtFontValid)
			{
				const std::wstring& comment = comments.at(i).str;
				GetTextSizeDW(comment, comment.length(), pDWR->pCommentTextFormat, pDWR, &size);

				_candidateCommentRects[i].SetRect(w, h, w + size.cx, h + size.cy);
				h += size.cy * cmtFontValid;
				height = max(height, h);
				wid = max(wid, size.cx);
			}
			else /* Used for highlighted candidate calculation below */
			{
				h -= space;
				_candidateCommentRects[i].SetRect(w, h, w, h);
				height = max(height, h);
				wid = max(wid, size.cx);
			}
			_candidateCommentRects[i].OffsetRect(offsetX, offsetY);

			_candidateRects[i].left = min(_candidateLabelRects[i].left, _candidateTextRects[i].left);
			_candidateRects[i].left = min(_candidateRects[i].left, _candidateCommentRects[i].left);
			_candidateRects[i].right = max(_candidateLabelRects[i].right, _candidateTextRects[i].right);
			_candidateRects[i].right = max(_candidateRects[i].right, _candidateCommentRects[i].right);
			_candidateRects[i].top = _candidateLabelRects[i].top;
			if (!comments.at(i).str.empty() && cmtFontValid)
				_candidateRects[i].bottom = _candidateCommentRects[i].bottom;
			else
				_candidateRects[i].bottom = _candidateTextRects[i].bottom;

			max_bot_candidate_rect = max(max_bot_candidate_rect, _candidateRects[i].bottom);
			w += wid + _style.candidate_spacing;
		}
		for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
			_candidateRects[i].bottom = max_bot_candidate_rect;
		w -= _style.candidate_spacing;
	}
	//height = max(height, h);
	width = max(width, w);

	/* Trim the last spacing */
	height += real_margin_y;
	width += real_margin_x;

	for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
	{
		if(_candidateRects[i].bottom < height - real_margin_y)
			_candidateRects[i].bottom = height - real_margin_y;
	}

	if (!IsInlinePreedit() && !_context.preedit.str.empty())
		_preeditRect.OffsetRect((width - real_margin_x) - _preeditRect.right, 0);
	if (!_context.aux.str.empty())
		_auxiliaryRect.OffsetRect((width - real_margin_x - _auxiliaryRect.right), 0);
	if (candidates.size())
	{
		for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
		{
			if (i == 0)
			{
				if (!IsInlinePreedit() && !_context.preedit.str.empty())
				{
					_candidateLabelRects[i].OffsetRect((_preeditRect.left - _style.spacing) - _candidateRects[i].right, 0);
					_candidateTextRects[i].OffsetRect((_preeditRect.left - _style.spacing) - _candidateRects[i].right, 0);
					_candidateCommentRects[i].OffsetRect((_preeditRect.left - _style.spacing) - _candidateRects[i].right, 0);
					_candidateRects[i].OffsetRect((_preeditRect.left - _style.spacing) - _candidateRects[i].right, 0);
				}
				else
				{
					_candidateLabelRects[i].OffsetRect((width - real_margin_x) - _candidateRects[i].right, 0);
					_candidateTextRects[i].OffsetRect((width - real_margin_x) - _candidateRects[i].right, 0);
					_candidateLabelRects[i].OffsetRect((width - real_margin_x) - _candidateRects[i].right, 0);
					_candidateRects[i].OffsetRect((width - real_margin_x) - _candidateRects[i].right, 0);
				}
			}
			else
			{
				_candidateLabelRects[i].OffsetRect((_candidateRects[i-1].left - _style.candidate_spacing) - _candidateRects[i].right, 0);
				_candidateTextRects[i].OffsetRect((_candidateRects[i-1].left - _style.candidate_spacing) - _candidateRects[i].right, 0);
				_candidateCommentRects[i].OffsetRect((_candidateRects[i-1].left - _style.candidate_spacing) - _candidateRects[i].right, 0);
				_candidateRects[i].OffsetRect((_candidateRects[i-1].left - _style.candidate_spacing) - _candidateRects[i].right, 0);
			}
		}
	}

	_highlightRect = _candidateRects[id];
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
	//_highlightRect = _candidateRects[id];

}

