
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

	int real_margin_x = (abs(_style.margin_x) > _style.hilite_padding) ? abs(_style.margin_x) : _style.hilite_padding;
	int real_margin_y = (abs(_style.margin_y) > _style.hilite_padding) ? abs(_style.margin_y) : _style.hilite_padding;
	int width = real_margin_x, height = real_margin_y;

	if (!_style.mark_text.empty() && (_style.hilited_mark_color & 0xff000000))
	{
		CSize sg;
		GetTextSizeDW(_style.mark_text, _style.mark_text.length(), pDWR->pTextFormat, pDWR, &sg);
		MARK_WIDTH = sg.cx;
		MARK_HEIGHT = sg.cy;
		MARK_GAP = MARK_HEIGHT + 4;
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
			_preeditRect.SetRect(width , height, width  + size.cx, height + size.cy);
			width += size.cx + _style.spacing;
		}
		_preeditRect.OffsetRect(offsetX, offsetY);
		height = max(height, real_margin_y + size.cy + real_margin_y);
	}

	/* Auxiliary */
	if (!_context.aux.str.empty())
	{
		size = GetPreeditSize(dc, _context.aux, pDWR->pTextFormat, pDWR);
		if(STATUS_ICON_SIZE/ 2 >= (width + size.cx / 2) && ShouldDisplayStatusIcon())
		{
			width += (STATUS_ICON_SIZE - size.cx) / 2;
			_auxiliaryRect.SetRect(width , height, width  + size.cx, height + size.cy);
			width += size.cx + (STATUS_ICON_SIZE - size.cx) / 2 + _style.spacing;
		}
		else
		{
			_auxiliaryRect.SetRect(width , height, width  + size.cx, height + size.cy);
			width += size.cx + _style.spacing;
		}
		_auxiliaryRect.OffsetRect(offsetX, offsetY);
		height = max(height, real_margin_y + size.cy + real_margin_y);
	}
	/* Candidates */
	int wids[MAX_CANDIDATES_COUNT] = {0};
	int w = width, h = real_margin_y, hh = 0, max_bot_candidate_rect = 0;
	int max_comment_heihgt = 0, max_content_height =  0, comment_shift_heihgt = 0;
	if (candidates.size() == 0)
		w -= _style.spacing;
	else
	{
		for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
		{
			int wid = 0;
			h = real_margin_y + base_offset;
			/* Label */
			std::wstring label = GetLabelText(labels, i, _style.label_text_format.c_str());
			GetTextSizeDW(label, label.length(), pDWR->pLabelTextFormat, pDWR, &size);
			_candidateLabelRects[i].SetRect(w, h, w + size.cx * labelFontValid, h + size.cy);
			h += size.cy * labelFontValid;
			max_content_height = max(max_content_height, h);
			wid = max(wid, size.cx);

			/* Text */
			h += _style.hilite_spacing;
			const std::wstring& text = candidates.at(i).str;
			GetTextSizeDW(text, text.length(), pDWR->pTextFormat, pDWR, &size);
			_candidateTextRects[i].SetRect(w, h, w + size.cx * textFontValid, h + size.cy);
			h += size.cy * textFontValid;
			max_content_height = max(max_content_height, h);
			wid = max(wid, size.cx);

			/* Comment */
			if (!comments.at(i).str.empty() && cmtFontValid)
			{
				h += _style.hilite_spacing;
				const std::wstring& comment = comments.at(i).str;
				GetTextSizeDW(comment, comment.length(), pDWR->pCommentTextFormat, pDWR, &size);
				_candidateCommentRects[i].SetRect(w, 0, w + size.cx * cmtFontValid, size.cy * cmtFontValid);
				h += size.cy * cmtFontValid;
				wid = max(wid, size.cx);
			}
			else /* Used for highlighted candidate calculation below */
			{
				_candidateCommentRects[i].SetRect(w, 0, w, 0);
				wid = max(wid, size.cx);
			}
			wids[i] = wid;
			max_comment_heihgt = max(max_comment_heihgt, _candidateCommentRects[i].Height());
			w += wid + _style.candidate_spacing;
		}
		w -= _style.candidate_spacing;
	}

	width = max(width, w);
	width += real_margin_x;

	// reposition candidates
	if (candidates.size())
	{
		for (size_t i = 0; i < candidates.size() && i < MAX_CANDIDATES_COUNT; ++i)
		{
			int ol = 0, ot = 0, oc = 0;
			ol = (wids[i] - _candidateLabelRects[i].Width()) / 2;
			ot = (wids[i] - _candidateTextRects[i].Width()) / 2;
			oc = (wids[i] - _candidateCommentRects[i].Width()) / 2;
			// offset rects
			_candidateLabelRects[i].OffsetRect(ol + offsetX, offsetY);
			_candidateTextRects[i].OffsetRect(ot + offsetX, offsetY);
			_candidateCommentRects[i].OffsetRect(oc + offsetX, offsetY + max_content_height + _style.hilite_spacing);
			// define  _candidateRects
			_candidateRects[i].left = min(_candidateLabelRects[i].left, _candidateTextRects[i].left);
			_candidateRects[i].left = min(_candidateRects[i].left, _candidateCommentRects[i].left);
			_candidateRects[i].right = max(_candidateLabelRects[i].right, _candidateTextRects[i].right);
			_candidateRects[i].right = max(_candidateRects[i].right, _candidateCommentRects[i].right);
			_candidateRects[i].top = _candidateLabelRects[i].top - base_offset;
			_candidateRects[i].bottom = _candidateCommentRects[i].top + max_comment_heihgt;
		}
		height = max(height, _candidateRects[0].Height() + 2*real_margin_y);

		if(!_style.vertical_text_left_to_right)
		{
			// re position right to left
			int base_left;
			if ((!IsInlinePreedit() && !_context.preedit.str.empty()))
				base_left = _preeditRect.left;
			else if( !_context.aux.str.empty())
				base_left = _auxiliaryRect.left;	
			else
				base_left = _candidateRects[0].left;
			for(int i = candidates.size() - 1; i>=0 ; i --)
			{
				int offset;
				if(i == candidates.size() - 1)
					offset = base_left - _candidateRects[i].left;
				else
					offset = _candidateRects[i+1].right + _style.candidate_spacing - _candidateRects[i].left;
				_candidateRects[i].OffsetRect(offset, 0);
				_candidateLabelRects[i].OffsetRect(offset, 0);
				_candidateTextRects[i].OffsetRect(offset, 0);
				_candidateCommentRects[i].OffsetRect(offset, 0);
			}
			if (!IsInlinePreedit() && !_context.preedit.str.empty())
				_preeditRect.OffsetRect(_candidateRects[0].right + _style.spacing - _preeditRect.left, 0);
			if (!_context.aux.str.empty())
				_auxiliaryRect.OffsetRect(_candidateRects[0].right + _style.spacing - _auxiliaryRect.left, 0);
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

}

