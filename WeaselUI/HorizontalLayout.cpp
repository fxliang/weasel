#include "stdafx.h"
#include "HorizontalLayout.h"

using namespace weasel;

HorizontalLayout::HorizontalLayout(const UIStyle &style, const Context &context, const Status &status)
	: StandardLayout(style, context, status)
{
}

void HorizontalLayout::DoLayout(CDCHandle dc, DirectWriteResources* pDWR )
{
	CSize size;
	const int space = _style.hilite_spacing;
	int width = 0, height = real_margin_y;

	/* calc mark_text sizes */
	if (!_style.mark_text.empty() && (_style.hilited_mark_color & 0xff000000))
	{
		CSize sg;
		GetTextSizeDW(_style.mark_text, _style.mark_text.length(), pDWR->pTextFormat, pDWR, &sg);
		MARK_WIDTH = sg.cx;
		MARK_HEIGHT = sg.cy;
		MARK_GAP = MARK_WIDTH + 4;
	}
	int base_offset =  ((_style.hilited_mark_color & 0xff000000) && !_style.mark_text.empty()) ? MARK_GAP : 0;

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
	
	int row_cnt = 0;
	int rows[MAX_CANDIDATES_COUNT] = {0};
	int max_width_of_rows = 0;
	int height_of_rows[MAX_CANDIDATES_COUNT] = {0};
	int mintops[MAX_CANDIDATES_COUNT] = {0};
	int maxbots[MAX_CANDIDATES_COUNT] = {0};
	int row_of_candidate[MAX_CANDIDATES_COUNT] = {0};
	int mintop_of_rows[MAX_CANDIDATES_COUNT] = {0};
	if(candidates_count)
	{
		int w = real_margin_x;
		int wrap = 0;

		for (size_t i = 0; i < candidates_count && i < MAX_CANDIDATES_COUNT; ++i)
		{
			int current_cand_width = 0;
			if( id == i )
			{
				w += base_offset;
				current_cand_width += base_offset;
			}
			/* Label */
			std::wstring label = GetLabelText(labels, i, _style.label_text_format.c_str());
			GetTextSizeDW(label, label.length(), pDWR->pLabelTextFormat, pDWR, &size);
			_candidateLabelRects[i].SetRect(w, height, w + size.cx * labelFontValid, height + size.cy);
			_candidateLabelRects[i].OffsetRect(offsetX, offsetY);
			w += size.cx * labelFontValid;
			current_cand_width += size.cx * labelFontValid;
			//height_of_rows[row_cnt] = max(height_of_rows[row_cnt], size.cy * labelFontValid);

			/* Text */
			w += _style.hilite_spacing;
			const std::wstring& text = candidates.at(i).str;
			GetTextSizeDW(text, text.length(), pDWR->pTextFormat, pDWR, &size);
			_candidateTextRects[i].SetRect(w, height, w + size.cx * textFontValid, height + size.cy);
			_candidateTextRects[i].OffsetRect(offsetX, offsetY);
			w += size.cx * textFontValid;
			current_cand_width += size.cx * textFontValid;
			//height_of_rows[row_cnt] = max(height_of_rows[row_cnt], size.cy * textFontValid);

			/* Comment */
			if (!comments.at(i).str.empty() && cmtFontValid )
			{
				const std::wstring& comment = comments.at(i).str;
				GetTextSizeDW(comment, comment.length(), pDWR->pCommentTextFormat, pDWR, &size);
				w += _style.hilite_spacing;
				_candidateCommentRects[i].SetRect(w, height, w + size.cx * cmtFontValid, height + size.cy);
				w += size.cx * cmtFontValid;
				current_cand_width += size.cx * cmtFontValid;
				//height_of_rows[row_cnt] = max(height_of_rows[row_cnt], size.cy);
			}
			else /* Used for highlighted candidate calculation below */
			{
				_candidateCommentRects[i].SetRect(w, height, w, height + size.cy);
			}
			_candidateCommentRects[i].OffsetRect(offsetX, offsetY);

			int base_left = (i==id) ? _candidateLabelRects[i].left - base_offset : _candidateLabelRects[i].left;
			// if not the first candidate of current row, and current_cand_width > _style.max_width
			if((base_left > real_margin_x + offsetX) && (_candidateCommentRects[i].right - offsetX + real_margin_x > _style.max_width))
			{
				max_width_of_rows = max(max_width_of_rows, _candidateCommentRects[i-1].right);
				w = offsetX + real_margin_x + (i==id? base_offset: 0);
				int ofx = w - _candidateLabelRects[i].left;
				int ofy = height_of_rows[row_cnt] + _style.candidate_spacing;
				height += ofy;
				w += current_cand_width;
				if (i < candidates_count - 1)	w += _style.candidate_spacing;
				
				_candidateLabelRects[i].OffsetRect(ofx, ofy);
				_candidateTextRects[i].OffsetRect(ofx, ofy);
				_candidateCommentRects[i].OffsetRect(ofx, ofy);
				row_cnt ++;
				height_of_rows[row_cnt] = max(height_of_rows[row_cnt], _candidateLabelRects[i].Height());
				height_of_rows[row_cnt] = max(height_of_rows[row_cnt], _candidateTextRects[i].Height());
				height_of_rows[row_cnt] = max(height_of_rows[row_cnt], _candidateCommentRects[i].Height());

			}
			else
			{
				height_of_rows[row_cnt] = max(height_of_rows[row_cnt], _candidateLabelRects[i].Height());
				height_of_rows[row_cnt] = max(height_of_rows[row_cnt], _candidateTextRects[i].Height());
				height_of_rows[row_cnt] = max(height_of_rows[row_cnt], _candidateCommentRects[i].Height());
				max_width_of_rows = max(max_width_of_rows, w);
				if (i < candidates_count - 1)	w += _style.candidate_spacing;
			}
			row_of_candidate[i] = row_cnt;
			mintops[i] = _candidateTextRects[i].bottom;
			mintops[i] = min(mintops[i], _candidateLabelRects[i].top);
			mintops[i] = min(mintops[i], _candidateCommentRects[i].top);
			maxbots[i] = _candidateTextRects[i].top;
			maxbots[i] = max(maxbots[i], _candidateLabelRects[i].bottom);
			maxbots[i] = max(maxbots[i], _candidateCommentRects[i].bottom);

			mintop_of_rows[row_cnt] = maxbots[i];
			mintop_of_rows[row_cnt] = min(mintop_of_rows[row_cnt], mintops[i]);
		}	

		for (size_t i = 0; i < candidates_count && i < MAX_CANDIDATES_COUNT; ++i)
		{
			int base_left = (i==id) ? _candidateLabelRects[i].left - base_offset : _candidateLabelRects[i].left;
			_candidateRects[i].SetRect(base_left, mintop_of_rows[row_of_candidate[i]],
					_candidateCommentRects[i].right, mintop_of_rows[row_of_candidate[i]] + height_of_rows[row_of_candidate[i]]);
			int ol = 0, ot = 0, oc = 0;
			if (_style.align_type == UIStyle::ALIGN_CENTER)
			{
				ol = (height_of_rows[row_of_candidate[i]] - _candidateLabelRects[i].Height()) / 2;
				ot = (height_of_rows[row_of_candidate[i]] - _candidateTextRects[i].Height()) / 2;
				oc = (height_of_rows[row_of_candidate[i]] - _candidateCommentRects[i].Height()) / 2;
			}
			else if (_style.align_type == UIStyle::ALIGN_BOTTOM)
			{
				ol = (height_of_rows[row_of_candidate[i]] - _candidateLabelRects[i].Height()) ;
				ot = (height_of_rows[row_of_candidate[i]] - _candidateTextRects[i].Height()) ;
				oc = (height_of_rows[row_of_candidate[i]] - _candidateCommentRects[i].Height()) ;
			}
			_candidateLabelRects[i].OffsetRect(0, ol);
			_candidateTextRects[i].OffsetRect(0, ot);
			_candidateCommentRects[i].OffsetRect(0, oc);
		}
	}
	
	_highlightRect = _candidateRects[id];
	width = max(width, max_width_of_rows);
	width += real_margin_x;
	
	height += height_of_rows[row_cnt];
	if(!candidates_count)
		height -= _style.spacing;

	height += real_margin_y;

	if (!_context.preedit.str.empty() && !candidates_count)
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
	// calc roundings end
}
