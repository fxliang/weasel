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
	int candidate_cnt = candidates.size();
	int id = _context.cinfo.highlighted;
	CSize size;
	const int space = _style.hilite_spacing;
	int real_margin_x = (abs(_style.margin_x) > _style.hilite_padding) ? abs(_style.margin_x) : _style.hilite_padding;
	int real_margin_y = (abs(_style.margin_y) > _style.hilite_padding) ? abs(_style.margin_y) : _style.hilite_padding;
	int width = 0, height = real_margin_y;

	CFont labelFont, textFont, commentFont;
	CFontHandle oldFont;
	/* create CFont for GDI mode */
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
	/* calc mark_text sizes */
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
	int row_cnt = 0;
	int rows[MAX_CANDIDATES_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	if(candidate_cnt)
	{
		int mintops[MAX_CANDIDATES_COUNT] = { 0 };
		int maxbots[MAX_CANDIDATES_COUNT] = { 0 };
		int w = real_margin_x;
		int wrap = 0;
		int height_of_rows[MAX_CANDIDATES_COUNT] = {0};
		int labelSizeValid = (pFonts->m_LabelFont.m_FontPoint > 0) ? 1 : 0;
		int textSizeValid = (pFonts->m_TextFont.m_FontPoint > 0) ? 1 : 0;
		int commentSizeValid = (pFonts->m_TextFont.m_FontPoint > 0) ? 1 : 0;
		for (size_t i = 0; i < candidate_cnt && i < MAX_CANDIDATES_COUNT; ++i)
		{
			if (i == id)
				w += base_offset;
			if (i > 0)
				w += _style.candidate_spacing - 1;
			int tmp = w;
			/* Label */
			std::wstring label = GetLabelText(labels, i, _style.label_text_format.c_str());
			if (_style.color_font)
				GetTextSizeDW(label, label.length(), pDWR->pLabelTextFormat, pDWR, &size);
			else
			{
				oldFont = dc.SelectFont(labelFont);
				GetTextExtentDCMultiline(dc, label, label.length(), &size);
			}

			_candidateLabelRects[i].SetRect(w, height, w + size.cx * labelSizeValid, height + size.cy);
			_candidateLabelRects[i].OffsetRect(offsetX, offsetY);
			w += (size.cx + space) * labelSizeValid;
			//h = max(h, size.cy);
			height_of_rows[row_cnt] = max(height_of_rows[row_cnt], size.cy);

			/* Text */
			const std::wstring& text = candidates.at(i).str;
			if (_style.color_font)
				GetTextSizeDW(text, text.length(), pDWR->pTextFormat, pDWR, &size);
			else
			{
				oldFont = dc.SelectFont(textFont);
				GetTextExtentDCMultiline(dc, text, text.length(), &size);
			}

			_candidateTextRects[i].SetRect(w, height, w + size.cx * textSizeValid, height + size.cy);
			_candidateTextRects[i].OffsetRect(offsetX, offsetY);
			w += (size.cx + space) * textSizeValid;
			height_of_rows[row_cnt] = max(height_of_rows[row_cnt], size.cy);

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
				w += (size.cx + space) * commentSizeValid;
				height_of_rows[row_cnt] = max(height_of_rows[row_cnt], size.cy);
			}
			else /* Used for highlighted candidate calculation below */
			{
				_candidateCommentRects[i].SetRect(w, height, w, height + size.cy);
			}
			_candidateCommentRects[i].OffsetRect(offsetX, offsetY);

			if(i != 0 && _style.layout_type != UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN && _style.max_width > 0 &&
					(  _candidateLabelRects[i].left	+ 2 * offsetX + real_margin_x		> _style.max_width 
					   || _candidateLabelRects[i].right	+ 2 * offsetX + real_margin_x	> _style.max_width 
					   || _candidateTextRects[i].left	+ 2 * offsetX + real_margin_x		> _style.max_width 
					   || _candidateTextRects[i].right	+ 2 * offsetX + real_margin_x		> _style.max_width 
					   || _candidateCommentRects[i].left + 2 * offsetX + real_margin_x		> _style.max_width 
					   || _candidateCommentRects[i].right + 2 * offsetX + real_margin_x	> _style.max_width
					)
			  )
			{
				row_cnt++;
				wrap = max(wrap, tmp);
				w = real_margin_x;
				if (i == id)
				{
					w += base_offset;
					wrap -= base_offset;
				}

				height += height_of_rows[row_cnt - 1] + _style.candidate_spacing;
				int ofx =   w - tmp;
				_candidateLabelRects[i].OffsetRect(ofx, height_of_rows[row_cnt - 1] + _style.candidate_spacing);
				_candidateTextRects[i].OffsetRect(ofx, height_of_rows[row_cnt - 1] + _style.candidate_spacing);
				_candidateCommentRects[i].OffsetRect(ofx, height_of_rows[row_cnt - 1] + _style.candidate_spacing);
				w += _candidateTextRects[i].Width()*textSizeValid + space + _candidateLabelRects[i].Width() * labelSizeValid + space 
					+ _candidateCommentRects[i].Width() * commentSizeValid;
				height_of_rows[row_cnt] = max(height_of_rows[row_cnt], _candidateLabelRects[i].Height());
				height_of_rows[row_cnt] = max(height_of_rows[row_cnt], _candidateTextRects[i].Height());
				height_of_rows[row_cnt] = max(height_of_rows[row_cnt], _candidateCommentRects[i].Height());

				if(!candidates.at(i).empty())
					mintops[row_cnt] = _candidateTextRects[i].bottom;
				if(!label.empty())
					mintops[row_cnt] = max(mintops[row_cnt], _candidateLabelRects[i].bottom);
				if(!comments.at(i).empty())
					mintops[row_cnt] = max(mintops[row_cnt], _candidateCommentRects[i].bottom);

				if(!candidates.at(i).empty())
					maxbots[row_cnt] = _candidateTextRects[i].top;
				if(!label.empty())
					maxbots[row_cnt] = min(maxbots[row_cnt], _candidateLabelRects[i].top);
				if(!comments.at(i).empty())
					maxbots[row_cnt] = min(maxbots[row_cnt], _candidateCommentRects[i].top);
			}
			else
			{
				wrap = _candidateCommentRects[i].right;
			}
			if(i == 0)
			{
				if(!candidates.at(i).empty())
					mintops[row_cnt] = _candidateTextRects[i].bottom;
				if(!label.empty())
					mintops[row_cnt] = max(mintops[row_cnt], _candidateLabelRects[i].bottom);
				if(!comments.at(i).empty())
					mintops[row_cnt] = max(mintops[row_cnt], _candidateCommentRects[i].bottom);

				if(!candidates.at(i).empty())
					maxbots[row_cnt] = _candidateTextRects[i].top;
				if(!label.empty())
					maxbots[row_cnt] = min(maxbots[row_cnt], _candidateLabelRects[i].top);
				if(!comments.at(i).empty())
					maxbots[row_cnt] = min(maxbots[row_cnt], _candidateCommentRects[i].top);
			}
			rows[i] = row_cnt;
			width = max(width, wrap);
		}

		if(!_style.color_font)
			dc.SelectFont(oldFont);

		int newhs[MAX_CANDIDATES_COUNT] = {0};
		for (size_t i = 0; i < candidate_cnt && i < MAX_CANDIDATES_COUNT; ++i)
		{
			int ol = 0, ot = 0, oc = 0;
			if (_style.align_type == UIStyle::ALIGN_CENTER)
			{
				ol = (height_of_rows[rows[i]] - _candidateLabelRects[i].Height()) / 2;
				ot = (height_of_rows[rows[i]] - _candidateTextRects[i].Height()) / 2;
				oc = (height_of_rows[rows[i]] - _candidateCommentRects[i].Height()) / 2;
			}
			else if (_style.align_type == UIStyle::ALIGN_BOTTOM)
			{
				ol = (height_of_rows[rows[i]] - _candidateLabelRects[i].Height()) ;
				ot = (height_of_rows[rows[i]] - _candidateTextRects[i].Height()) ;
				oc = (height_of_rows[rows[i]] - _candidateCommentRects[i].Height()) ;
			}
			_candidateLabelRects[i].OffsetRect(0, ol);
			_candidateTextRects[i].OffsetRect(0, ot);
			_candidateCommentRects[i].OffsetRect(0, oc);

			mintops[rows[i]] = min(mintops[rows[i]], _candidateLabelRects[i].top);
			mintops[rows[i]] = min(mintops[rows[i]], _candidateTextRects[i].top);
			mintops[rows[i]] = min(mintops[rows[i]], _candidateCommentRects[i].top);
			maxbots[rows[i]] = max(maxbots[rows[i]], _candidateLabelRects[i].bottom);
			maxbots[rows[i]] = max(maxbots[rows[i]], _candidateTextRects[i].bottom);
			maxbots[rows[i]] = max(maxbots[rows[i]], _candidateCommentRects[i].bottom);
			newhs[rows[i]] = min(newhs[rows[i]], ol);
			newhs[rows[i]] = min(newhs[rows[i]], ot);
			newhs[rows[i]] = min(newhs[rows[i]], oc);
			if((i != candidate_cnt - 1 && rows[i+1] > rows[i]) || (i == candidate_cnt - 1)) height_of_rows[rows[i]] -= newhs[rows[i]];
		}

		/* Highlighted Candidate */
		for (size_t i = 0; i < candidate_cnt && i < MAX_CANDIDATES_COUNT; ++i)
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
			hlTop = min(mintops[rows[i]], hlTop);
			hlBot = max(maxbots[rows[i]], hlBot);
			_candidateRects[i].SetRect(_candidateLabelRects[i].left, hlTop, _candidateCommentRects[i].right, hlBot);
			if(id == i) _candidateRects[i].left -= base_offset;
		}

		width = max(width, w);
		height += height_of_rows[0];
		height = min(height, _candidateRects[candidate_cnt - 1].bottom);
	}
	else
		height -= _style.spacing;
	height += real_margin_y;
	width += real_margin_x;

	if (!_context.preedit.str.empty() && !candidates.empty())
	{
		width = max(width, _style.min_width);
		height = max(height, _style.min_height);
	}
	UpdateStatusIconLayout(&width, &height);
	_contentSize.SetSize(width + 2 * offsetX, height + 2 * offsetY);

	/* right of last _candidateRects of every row should be the same */
	if(candidate_cnt)
	{
		for(int i = 0; i<candidate_cnt ; i++)
		{
			if((i != candidate_cnt - 1 && rows[i] != rows[i+1]) || (i == candidate_cnt - 1))
				_candidateRects[i].right = width - real_margin_x + offsetX;
		}
		_highlightRect = _candidateRects[id];
	}

	// calc roundings start
	_contentRect.SetRect(0, 0, _contentSize.cx, _contentSize.cy);
	CopyRect(_bgRect, _contentRect);
	_bgRect.DeflateRect(offsetX + 1, offsetY + 1);
	_PrepareRoundInfo(dc);
	bool textHemispherical = false, cand0Hemispherical = false;
	if(!_style.inline_preedit)
	{
		CRect textRect(_preeditRect);
		textRect.InflateRect(_style.hilite_padding, _style.hilite_padding);
		textHemispherical = _IsHighlightOverCandidateWindow(textRect, dc);
	}
	if(candidate_cnt)
	{
		CRect cand0Rect(_candidateRects[0]);
		cand0Rect.InflateRect(_style.hilite_padding, _style.hilite_padding);
		cand0Hemispherical = _IsHighlightOverCandidateWindow(cand0Rect, dc);
		if(textHemispherical || cand0Hemispherical)
			for (int i = 0; i < candidate_cnt; i++)
			{
				if((i != (candidate_cnt - 1)) && (i != 0) && (rows[i+1] == rows[i-1]))	// not the first or last
				{
					_roundInfo[i].IsTopLeftNeedToRound = false;
					_roundInfo[i].IsTopRightNeedToRound = false;
					_roundInfo[i].IsBottomLeftNeedToRound = false;
					_roundInfo[i].IsBottomRightNeedToRound = false;
				}

				if (i == 0)	// first cand
				{
					_roundInfo[i].IsTopLeftNeedToRound = _roundInfo[i].IsTopLeftNeedToRound && _style.inline_preedit;
					_roundInfo[i].IsBottomLeftNeedToRound = _roundInfo[i].IsBottomLeftNeedToRound && (row_cnt == 0);
				}
				if (i < candidate_cnt - 1 && rows[i] != rows[i + 1])	// row end, not last
				{
					_roundInfo[i].IsBottomRightNeedToRound = false;
					_roundInfo[i].IsTopRightNeedToRound = (_style.inline_preedit && rows[i] == 0);
				}
				if (i > 0 && rows[i] == row_cnt && rows[i - 1] == (row_cnt - 1))	// last line start
				{
					_roundInfo[i].IsTopLeftNeedToRound = false;
					_roundInfo[i].IsBottomLeftNeedToRound = true;
				}
				if (i == candidate_cnt - 1)	// last candidate
				{
					if(rows[i])
						_roundInfo[i].IsTopRightNeedToRound = false;
				}

			}
	}
	int deflatex = offsetX - _style.border / 2;
	int deflatey = offsetY - _style.border / 2;
	_contentRect.DeflateRect(deflatex, deflatey);
	if (_style.border % 2 == 0)	_contentRect.DeflateRect(1, 1);
	// calc roundings end
	
	labelFont.DeleteObject();
	textFont.DeleteObject();
	commentFont.DeleteObject();
	oldFont.DeleteObject();
}
