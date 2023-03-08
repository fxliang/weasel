#include "stdafx.h"
#include "StandardLayout.h"

using namespace weasel;

StandardLayout::StandardLayout(const UIStyle &style, const Context &context, const Status &status)
	: Layout(style, context, status)
{
}
std::wstring StandardLayout::GetLabelText(const std::vector<Text> &labels, int id, const wchar_t *format) const
{
	wchar_t buffer[128];
	swprintf_s<128>(buffer, format, labels.at(id).str.c_str());
	return std::wstring(buffer);
}

void weasel::StandardLayout::GetTextSizeDW(const std::wstring text, int nCount, IDWriteTextFormat1* pTextFormat, DirectWriteResources* pDWR,  LPSIZE lpSize) const
{
	D2D1_SIZE_F sz;
	HRESULT hr = S_OK;
	//pDWR->pTextLayout = NULL;

	if (pTextFormat == NULL)
	{
		lpSize->cx = 0;
		lpSize->cy = 0;
		return;
	}
	// �����ı����� 
	if (pTextFormat != NULL)
		if (_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT)
			hr = pDWR->pDWFactory->CreateTextLayout(text.c_str(), nCount, pTextFormat, 0, _style.max_height, reinterpret_cast<IDWriteTextLayout**>(&pDWR->pTextLayout));
		else
			hr = pDWR->pDWFactory->CreateTextLayout(text.c_str(), nCount, pTextFormat, _style.max_width, 0, reinterpret_cast<IDWriteTextLayout**>(&pDWR->pTextLayout));

	if (SUCCEEDED(hr))
	{
		if (_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT)
		{
			DWRITE_FLOW_DIRECTION flow = _style.vertical_text_left_to_right ? DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT : DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT;
			pDWR->pTextLayout->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
			pDWR->pTextLayout->SetFlowDirection(flow);
		}
		// ��ȡ�ı��ߴ�  
		DWRITE_TEXT_METRICS textMetrics;
		hr = pDWR->pTextLayout->GetMetrics(&textMetrics);
		sz = D2D1::SizeF(ceil(textMetrics.width), ceil(textMetrics.height));

		lpSize->cx = (int)sz.width;
		lpSize->cy = (int)sz.height;
		SafeRelease(&pDWR->pTextLayout);
		size_t max_width;
		
		max_width = _style.max_width == 0 ? textMetrics.widthIncludingTrailingWhitespace : _style.max_width;
		hr = pDWR->pDWFactory->CreateTextLayout(text.c_str(), nCount, pTextFormat, textMetrics.width, textMetrics.height,  reinterpret_cast<IDWriteTextLayout**>(&pDWR->pTextLayout));

		if (_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT)
		{
			pDWR->pTextLayout->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
			pDWR->pTextLayout->SetFlowDirection(DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT);
		}
		DWRITE_OVERHANG_METRICS overhangMetrics;
		hr = pDWR->pTextLayout->GetOverhangMetrics(&overhangMetrics);
		{
			if (overhangMetrics.left > 0)
				lpSize->cx += overhangMetrics.left + 1;
			if (overhangMetrics.right > 0)
				lpSize->cx += overhangMetrics.right + 1;
			if (overhangMetrics.top > 0)
				lpSize->cy += overhangMetrics.top + 1;
			if (overhangMetrics.bottom > 0)
				lpSize->cy += overhangMetrics.bottom + 1;
		}
	}
	SafeRelease(&pDWR->pTextLayout);
}

CSize StandardLayout::GetPreeditSize(CDCHandle dc, const weasel::Text& text, IDWriteTextFormat1* pTextFormat, DirectWriteResources* pDWR) const
{
	const std::wstring& preedit = text.str;
	const std::vector<weasel::TextAttribute> &attrs = text.attributes;
	CSize size(0, 0);
	if (!preedit.empty())
	{
		GetTextSizeDW(preedit, preedit.length(), pTextFormat, pDWR, &size);
		for (size_t i = 0; i < attrs.size(); i++)
		{
			if (attrs[i].type == weasel::HIGHLIGHTED)
			{
				const weasel::TextRange &range = attrs[i].range;
				if (range.start < range.end)
				{
					if (_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT)
					{
						if (range.start > 0)
							size.cy += _style.hilite_spacing;
						else
							size.cy += _style.hilite_padding;
						if (range.end < static_cast<int>(preedit.length()))
							size.cy += _style.hilite_spacing;
						else
							size.cy += _style.hilite_padding;
					}
					else
					{
						if (range.start > 0)
							size.cx += _style.hilite_spacing;
						else
							size.cx += _style.hilite_padding;
						if (range.end < static_cast<int>(preedit.length()))
							size.cx += _style.hilite_spacing;
						else
							size.cx += _style.hilite_padding;
					}
				}
				// only one highlighted, break to save time break;
			}
		}
	}
	return size;
}
bool weasel::StandardLayout::_IsHighlightOverCandidateWindow(CRect& rc, CDCHandle& dc)
{
	GraphicsRoundRectPath bgPath(_bgRect, _style.round_corner_ex);
	GraphicsRoundRectPath hlPath(rc, _style.round_corner);

	Gdiplus::Region bgRegion(&bgPath);
	Gdiplus::Region hlRegion(&hlPath);
	Gdiplus::Region* tmpRegion = hlRegion.Clone();

	tmpRegion->Xor(&bgRegion);
	tmpRegion->Exclude(&bgRegion);

	Gdiplus::Graphics g(dc);
	bool res = !tmpRegion->IsEmpty(&g);
	delete tmpRegion;
	tmpRegion = NULL;
	return res;
}

void weasel::StandardLayout::_PrepareRoundInfo(CDCHandle& dc)
{
	bool textHemispherical = false, cand0Hemispherical = false;
	if(!_style.inline_preedit)
	{
		CRect textRect(_preeditRect);
		textRect.InflateRect(_style.hilite_padding, _style.hilite_padding);
		textHemispherical = _IsHighlightOverCandidateWindow(textRect, dc);
	}
	if(candidates_count)
	{
		CRect cand0Rect(_candidateRects[0]);
		cand0Rect.InflateRect(_style.hilite_padding, _style.hilite_padding);
		cand0Hemispherical = _IsHighlightOverCandidateWindow(cand0Rect, dc);
	}
	if(textHemispherical || cand0Hemispherical)
		for (auto i = 0; i < candidates_count; i++)
		{
			CRect hilite_rect(_candidateRects[i]);
			hilite_rect.InflateRect(_style.hilite_padding, _style.hilite_padding);
			bool current_hemispherical_dome_status = _IsHighlightOverCandidateWindow(hilite_rect, dc);
			//if (current_hemispherical_dome_status) _textRoundInfo.Hemispherical = true;
			// level 0: m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL
			// level 1: m_style.inline_preedit 
			// level 2: BackType
			// level 3: IsTopLeftNeedToRound, IsBottomLeftNeedToRound, IsTopRightNeedToRound, IsBottomRightNeedToRound
			const static bool is_to_round_corner[3][2][5][4] =
			{
				// LAYOUT_VERTICAL
				{
					// not inline_preedit
					{
						{current_hemispherical_dome_status, current_hemispherical_dome_status && (!candidates_count), false, false},		// TEXT
						{false, false, false, false},		// FIRST_CAND
						{false, false, false, false},	// MID_CAND
						{false, true, false, true},		// LAST_CAND
						{false, true, false, true},		// ONLY_CAND
					} ,
					// inline_preedit
					{
						{true, true, true, true},		// TEXT
						{true, false, true, false},		// FIRST_CAND
						{false, false, false, false},	// MID_CAND
						{false, true, false, true},		// LAST_CAND
						{true, true, true, true},		// ONLY_CAND
					}
				} ,
				// LAYOUT_HORIZONTAL
				{
					// not inline_preedit
					{
						{current_hemispherical_dome_status, current_hemispherical_dome_status && (!candidates_count), false, false},		// TEXT
						{false, true, false, false},		// FIRST_CAND
						{false, false, false, false},	// MID_CAND
						{false, false, false, true},		// LAST_CAND
						{false, true, false, true},		// ONLY_CAND
					} ,
					// inline_preedit
					{
						{true, true, true, true},		// TEXT
						{true, true, false, false},		// FIRST_CAND
						{false, false, false, false},	// MID_CAND
						{false, false, true, true},		// LAST_CAND
						{true, true, true, true},		// ONLY_CAND
					}
				},
				// LAYOUT_VERTICAL_TEXT
				{
					// not inline_preedit
					{
						//{current_hemispherical_dome_status, current_hemispherical_dome_status && (!candidates_count), false, false},		// TEXT
						{false, false, current_hemispherical_dome_status, current_hemispherical_dome_status && (!candidates_count)}, // false, false},		// TEXT
						{false, false, false, false},		// FIRST_CAND
						{false, false, false, false},	// MID_CAND
						{true, true, false, false},		// LAST_CAND
						{false, true, false, true},		// ONLY_CAND
					} ,
					// inline_preedit
					{
						{true, true, true, true},		// TEXT
						{true, true, false, false},		// FIRST_CAND
						{false, false, false, false},	// MID_CAND
						{false, false, true, true},		// LAST_CAND
						{true, true, true, true},		// ONLY_CAND
					}
				} ,
			};
			int type = 1;
			if (candidates_count == 1)
				type = 4;
			else if (i != 0 && i != candidates_count - 1)
				type = 2;
			else if (i == candidates_count - 1)
				type = 3;
			const int tmp[5]= { UIStyle::LAYOUT_VERTICAL, UIStyle::LAYOUT_HORIZONTAL, UIStyle::LAYOUT_VERTICAL_TEXT, UIStyle::LAYOUT_VERTICAL, UIStyle::LAYOUT_HORIZONTAL };
			int layout_type = tmp[_style.layout_type];
			_roundInfo[i].IsTopLeftNeedToRound = is_to_round_corner[layout_type][_style.inline_preedit][type][0];
			_roundInfo[i].IsBottomLeftNeedToRound = is_to_round_corner[layout_type][_style.inline_preedit][type][1];
			_roundInfo[i].IsTopRightNeedToRound = is_to_round_corner[layout_type][_style.inline_preedit][type][2];
			_roundInfo[i].IsBottomRightNeedToRound = is_to_round_corner[layout_type][_style.inline_preedit][type][3];
			_roundInfo[i].Hemispherical = current_hemispherical_dome_status;
			if (i == candidates_count - 1)
			{
				type = 0;
				_textRoundInfo.IsTopLeftNeedToRound = is_to_round_corner[layout_type][_style.inline_preedit][type][0];
				_textRoundInfo.IsBottomLeftNeedToRound = is_to_round_corner[layout_type][_style.inline_preedit][type][1];
				_textRoundInfo.IsTopRightNeedToRound = is_to_round_corner[layout_type][_style.inline_preedit][type][2];
				_textRoundInfo.IsBottomRightNeedToRound = is_to_round_corner[layout_type][_style.inline_preedit][type][3];
				_textRoundInfo.Hemispherical = textHemispherical;
			}
		}
}

void StandardLayout::UpdateStatusIconLayout(int* width, int* height)
{
	// rule 1. status icon is middle-aligned with preedit text or auxiliary text, whichever comes first
	// rule 2. there is a spacing between preedit/aux text and the status icon
	// rule 3. status icon is right aligned in WeaselPanel, when [margin_x + width(preedit/aux) + spacing + width(icon) + margin_x] < style.min_width
	int real_margin_x = (abs(_style.margin_x) > _style.hilite_padding) ? abs(_style.margin_x) : _style.hilite_padding;
	int real_margin_y = (abs(_style.margin_y) > _style.hilite_padding) ? abs(_style.margin_y) : _style.hilite_padding;
	if (ShouldDisplayStatusIcon())
	{
		int left = 0, middle = 0;
		if (!_preeditRect.IsRectNull())
		{
			left = _preeditRect.right + _style.spacing;
			middle = (_preeditRect.top + _preeditRect.bottom) / 2;
		}
		else if (!_auxiliaryRect.IsRectNull())
		{
			left = _auxiliaryRect.right + _style.spacing;
			middle = (_auxiliaryRect.top + _auxiliaryRect.bottom) / 2;
		}
		if (left && middle)
		{
			int right_alignment = *width - real_margin_x - STATUS_ICON_SIZE;
			if (left > right_alignment)
			{
				*width = left + STATUS_ICON_SIZE + real_margin_x;
			}
			else
			{
				left = right_alignment;
			}
			_statusIconRect.SetRect(left, middle - STATUS_ICON_SIZE / 2 + 1, left + STATUS_ICON_SIZE, middle + STATUS_ICON_SIZE / 2 + 1);
		}
		else
		{
			_statusIconRect.SetRect(0, 0, STATUS_ICON_SIZE, STATUS_ICON_SIZE);
			_statusIconRect.OffsetRect(offsetX, offsetY);
			*width = *height = STATUS_ICON_SIZE;
		}
	}
}

bool StandardLayout::IsInlinePreedit() const
{
	return _style.inline_preedit && (_style.client_caps & weasel::INLINE_PREEDIT_CAPABLE) != 0 &&
		_style.layout_type != UIStyle::LAYOUT_VERTICAL_FULLSCREEN && _style.layout_type != UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN;
}

bool StandardLayout::ShouldDisplayStatusIcon() const
{
	// rule 1. emphasis ascii mode
	// rule 2. show status icon when switching mode
	// rule 3. always show status icon with tips 
	return _status.ascii_mode || !_status.composing || !_context.aux.empty();
}
