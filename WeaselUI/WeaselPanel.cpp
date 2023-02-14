#include "stdafx.h"
#include "WeaselPanel.h"
#include <WeaselCommon.h>
#include "VersionHelpers.hpp"
#include "VerticalLayout.h"
#include "HorizontalLayout.h"
#include "FullScreenLayout.h"

// for IDI_ZH, IDI_EN
#include <resource.h>
#define PREPAREBITMAPINFOHEADER(width, height)	{40, width, height, 1, 32, BI_RGB, 0, 0, 0, 0, 0}
#define COLORTRANSPARENT(color)		((color & 0xff000000) == 0)
#define COLORNOTTRANSPARENT(color)	(color & 0xff000000)

WeaselPanel::WeaselPanel(weasel::UI& ui)
	: m_layout(NULL),
	m_ctx(ui.ctx()),
	m_octx(ui.octx()),
	m_status(ui.status()),
	m_style(ui.style()),
	m_ostyle(ui.ostyle()),
	m_candidateCount(0),
	hide_candidates(false),
	pDWR(NULL),
	pFonts(new GDIFonts(ui.style())),
	m_blurer(new GdiplusBlur()),
	setWindowCompositionAttribute(NULL),
	accent({ ACCENT_ENABLE_BLURBEHIND, 0xff, (DWORD)((long long)m_style.back_color), 0 }),
	data({ WCA_ACCENT_POLICY, &accent, sizeof(accent) }),
	_isWindows10OrGreater(IsWindows10OrGreaterEx()),
	hUser(ui.module()),
	pBrush(NULL),
	_m_gdiplusToken(0)
{
	m_iconDisabled.LoadIconW(IDI_RELOAD, STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_DEFAULTCOLOR);
	m_iconEnabled.LoadIconW(IDI_ZH, STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_DEFAULTCOLOR);
	m_iconAlpha.LoadIconW(IDI_EN, STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_DEFAULTCOLOR);
	m_iconFull.LoadIconW(IDI_FULL_SHAPE, STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_DEFAULTCOLOR);
	m_iconHalf.LoadIconW(IDI_HALF_SHAPE, STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_DEFAULTCOLOR);
	if(hUser && _isWindows10OrGreater)
		setWindowCompositionAttribute = (pfnSetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
	GdiplusStartup(&_m_gdiplusToken, &_m_gdiplusStartupInput, NULL);
	InitFontRes();
	m_ostyle = m_style;
}

WeaselPanel::~WeaselPanel()
{
	Gdiplus::GdiplusShutdown(_m_gdiplusToken);
	CleanUp();
}

void WeaselPanel::_ResizeWindow()
{
	CDCHandle dc = GetDC();
	CSize size = m_layout->GetContentSize();
	if(size != m_osize) {
		SetWindowPos(NULL, 0, 0, size.cx, size.cy, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
		m_osize = size;
	}
	ReleaseDC(dc);
}

void WeaselPanel::_CreateLayout()
{
	if (m_layout != NULL)
		delete m_layout;

	Layout* layout = NULL;
	if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL ||
		m_style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN)
	{
		layout = new VerticalLayout(m_style, m_ctx, m_status);
	}
	else if (m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL ||
		m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN)
	{
		layout = new HorizontalLayout(m_style, m_ctx, m_status);
	}
	if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN ||
		m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN)
	{
		layout = new FullScreenLayout(m_style, m_ctx, m_status, m_inputPos, layout);
	}
	m_layout = layout;
}

//更新界面
void WeaselPanel::Refresh()
{
	bool should_show_icon = (m_status.ascii_mode || !m_status.composing || !m_ctx.aux.empty());
	m_candidateCount = (BYTE)m_ctx.cinfo.candies.size();
	// check if to hide candidates window
	// show tips status, two kind of situation: 1) only aux strings, don't care icon status; 2)only icon(ascii mode switching)
	bool show_tips = (!m_ctx.aux.empty() && m_ctx.cinfo.empty() && m_ctx.preedit.empty()) || (m_ctx.empty() && should_show_icon);
	// show schema menu status: always preedit start with "〔方案選單〕"
	bool show_schema_menu = boost::regex_search(m_ctx.preedit.str, boost::wsmatch(), boost::wregex(L"^〔方案選單〕", boost::wregex::icase));
	bool margin_negative = (m_style.margin_x < 0 || m_style.margin_y < 0);
	bool inline_no_candidates = m_style.inline_preedit && (m_ctx.cinfo.candies.size() == 0) && (!show_tips);
	// when to hide_cadidates?
	// 1. inline_no_candidates
	// or
	// 2. margin_negative, and not in show tips mode( ascii switching / half-full switching / simp-trad switching / error tips), and not in schema menu
	hide_candidates = inline_no_candidates || (margin_negative && !show_tips && !show_schema_menu);

	_CreateLayout();

	InitFontRes();
	CDCHandle dc = GetDC();
	m_layout->DoLayout(dc, pFonts, pDWR);
	ReleaseDC(dc);
	_ResizeWindow();
	_RepositionWindow();
	if(!hide_candidates)
	{ 
		RedrawWindow();
	}
}

void WeaselPanel::InitFontRes(void)
{
	if (m_style.color_font)
	{
		// prepare d2d1 resources
		if (pDWR == NULL)
			pDWR = new DirectWriteResources(m_style);
		else if(m_ostyle != m_style)
			pDWR->InitResources(m_style);

		if(pBrush == NULL)
			pDWR->pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0, 1.0, 1.0, 1.0), &pBrush);
	}
	else
	{
		delete pFonts;
		pFonts = new GDIFonts(m_style);
	}
	m_ostyle = m_style;
}

void WeaselPanel::CleanUp()
{
	delete m_layout;
	m_layout = NULL;

	delete pDWR;
	pDWR = NULL;

	delete pFonts;
	pFonts = NULL;

	delete m_blurer;
	m_blurer = NULL;

	SafeRelease(&pBrush);
	pBrush = NULL;
}

void WeaselPanel::_HighlightText(CDCHandle dc, CRect rc, COLORREF color, COLORREF shadowColor, int radius, BackType type = BackType::TEXT, bool highlighted = false, 
	IsToRoundStruct rd = IsToRoundStruct())
{
	Gdiplus::Graphics g_back(dc);
	g_back.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);

	int blurMarginX = m_layout->offsetX * 3;
	int blurMarginY = m_layout->offsetY * 3;
	// 必须shadow_color都是非完全透明色才做绘制, 全屏状态不绘制阴影保证响应速度
	if ( m_style.shadow_radius && COLORNOTTRANSPARENT(shadowColor) && m_style.layout_type != UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN && m_style.layout_type != UIStyle::LAYOUT_VERTICAL_FULLSCREEN ) {
		CRect rect(
			blurMarginX + m_style.shadow_offset_x,
			blurMarginY + m_style.shadow_offset_y,
			rc.Width()  + blurMarginX + m_style.shadow_offset_x,
			rc.Height() + blurMarginY + m_style.shadow_offset_y);
		BYTE r = GetRValue(shadowColor);
		BYTE g = GetGValue(shadowColor);
		BYTE b = GetBValue(shadowColor);
		BYTE alpha = (BYTE)((shadowColor >> 24) & 255);
		Gdiplus::Color shadow_color = Gdiplus::Color::MakeARGB(alpha, r, g, b);
		static Gdiplus::Bitmap* pBitmapDropShadow;
		pBitmapDropShadow = new Gdiplus::Bitmap((INT)rc.Width() + blurMarginX * 2, (INT)rc.Height() + blurMarginY * 2, PixelFormat32bppARGB);

		Gdiplus::Graphics g_shadow(pBitmapDropShadow);
		g_shadow.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
		// dropshadow, draw a roundrectangle to blur
		if (m_style.shadow_offset_x != 0 || m_style.shadow_offset_y != 0) {
			GraphicsRoundRectPath shadow_path(rect, radius);
			Gdiplus::SolidBrush shadow_brush(shadow_color);
			g_shadow.FillPath(&shadow_brush, &shadow_path);
		}
		// round shadow, draw multilines as base round line
		else {
			int step = alpha /  m_style.shadow_radius;
			Gdiplus::Pen pen_shadow(shadow_color, (Gdiplus::REAL)1);
			for (int i = 0; i < m_style.shadow_radius; i++) {
				GraphicsRoundRectPath round_path(rect, radius + 1 + i);
				g_shadow.DrawPath(&pen_shadow, &round_path);
				shadow_color = Gdiplus::Color::MakeARGB(alpha - i * step, r, g, b);
				pen_shadow.SetColor(shadow_color);
				rect.InflateRect(1, 1);
			}
		}
		m_blurer->DoGaussianBlur(pBitmapDropShadow, (float)m_style.shadow_radius, (float)m_style.shadow_radius);
		g_back.DrawImage(pBitmapDropShadow, rc.left - blurMarginX, rc.top - blurMarginY);
		delete pBitmapDropShadow;
		pBitmapDropShadow = NULL;
	}
	// 必须back_color非完全透明才绘制
	if COLORNOTTRANSPARENT(color)	{
		Gdiplus::Color back_color = Gdiplus::Color::MakeARGB((color >> 24), GetRValue(color), GetGValue(color), GetBValue(color));
		Gdiplus::SolidBrush back_brush(back_color);
		GraphicsRoundRectPath* hiliteBackPath;
		// candidates only, and current candidate background out of window background
		if (rd.Hemispherical && type!= BackType::BACKGROUND  && m_style.layout_type != UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN && m_style.layout_type != UIStyle::LAYOUT_VERTICAL_FULLSCREEN) {
			int rr = m_style.round_corner_ex - m_style.border / 2 + (m_style.border % 2 == 0);
			hiliteBackPath = new GraphicsRoundRectPath(rc, rr, rd.IsTopLeftNeedToRound, rd.IsTopRightNeedToRound, rd.IsBottomRightNeedToRound, rd.IsBottomLeftNeedToRound);
		}
		// background or current candidate background not out of window background
		else
			hiliteBackPath = new GraphicsRoundRectPath(rc, radius);
		g_back.FillPath(&back_brush, hiliteBackPath);
		delete hiliteBackPath;
		hiliteBackPath = NULL;
	}
	// draw hilited mark
	if (highlighted && COLORNOTTRANSPARENT(m_style.hilited_mark_color))
	{
		if (m_style.mark_text.empty())
		{
			int topm = (rc.Height() - ((double)rc.Height() - (double)max(m_style.hilite_padding, m_style.round_corner) * 2) * 0.7) / 2;
			CRect hlRc(rc.left + m_style.hilite_padding + (m_layout->MARK_GAP - m_layout->MARK_WIDTH) / 2 + 1, rc.top + topm, 
				rc.left + m_style.hilite_padding + (m_layout->MARK_GAP + m_layout->MARK_WIDTH) / 2 + 1, rc.bottom - topm);
			GraphicsRoundRectPath hlp(hlRc, 4);
			Gdiplus::Color hlcl = Gdiplus::Color::MakeARGB(0xff, GetRValue(m_style.hilited_mark_color), GetGValue(m_style.hilited_mark_color), GetBValue(m_style.hilited_mark_color));
			Gdiplus::SolidBrush hl_brush(hlcl);
			g_back.FillPath(&hl_brush, &hlp);
		}
		else
		{
			IDWriteTextFormat1* txtFormat = (m_style.color_font) ? pDWR->pTextFormat : NULL;
			int vgap = m_layout->MARK_HEIGHT ? (rc.Height() - m_layout->MARK_HEIGHT) / 2 : 0;
			CRect hlRc(rc.left + m_style.hilite_padding + (m_layout->MARK_GAP - m_layout->MARK_WIDTH) / 2 + 1, rc.top + vgap, 
				rc.left + m_style.hilite_padding + (m_layout->MARK_GAP - m_layout->MARK_WIDTH) / 2 + 1 + m_layout->MARK_WIDTH, rc.bottom - vgap);
			_TextOut(dc, hlRc, m_style.mark_text.c_str(), m_style.mark_text.length(), &pFonts->m_TextFont, m_style.hilited_mark_color, txtFormat);
		}
	}
	// draw border
	if (type == BackType::BACKGROUND && m_style.border != 0 && COLORNOTTRANSPARENT(m_style.border_color)) {
		GraphicsRoundRectPath bgPath(rc, m_style.round_corner_ex);
		int alpha = ((m_style.border_color >> 24) & 0xff);
		Gdiplus::Color border_color = Gdiplus::Color::MakeARGB(alpha, GetRValue(m_style.border_color), GetGValue(m_style.border_color), GetBValue(m_style.border_color));
		Gdiplus::Pen gPenBorder(border_color, (Gdiplus::REAL)m_style.border);
		g_back.DrawPath(&gPenBorder, &bgPath);
	}
}

bool WeaselPanel::_DrawPreedit(Text const& text, CDCHandle dc, CRect const& rc)
{
	bool drawn = false;
	std::wstring const& t = text.str;
	IDWriteTextFormat1* txtFormat = m_style.color_font? pDWR->pTextFormat : NULL;
	if (!t.empty()) {
		weasel::TextRange range;
		std::vector<weasel::TextAttribute> const& attrs = text.attributes;
		for (size_t j = 0; j < attrs.size(); ++j)
			if (attrs[j].type == weasel::HIGHLIGHTED)
				range = attrs[j].range;

		if (range.start < range.end) {
			CSize selStart, selEnd;
			if (m_style.color_font) {
				m_layout->GetTextSizeDW(t, range.start, pDWR->pTextFormat, pDWR, &selStart);
				m_layout->GetTextSizeDW(t, range.end, pDWR->pTextFormat, pDWR, &selEnd);
			}
			else {
				long height = -MulDiv(pFonts->m_TextFont.m_FontPoint, dc.GetDeviceCaps(LOGPIXELSY), 72);
				CFont font;
				CFontHandle oldFont;
				font.CreateFontW(height, 0, 0, 0, pFonts->m_TextFont.m_FontWeight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, pFonts->m_TextFont.m_FontFace.c_str());
				oldFont = dc.SelectFont(font);
				m_layout->GetTextExtentDCMultiline(dc, t, range.start, &selStart);
				m_layout->GetTextExtentDCMultiline(dc, t, range.end, &selEnd);
				dc.SelectFont(oldFont);
				font.DeleteObject();
				oldFont.DeleteObject();
			}
			int x = rc.left;
			if (range.start > 0) {
				// zzz
				std::wstring str_before(t.substr(0, range.start));
				CRect rc_before(x, rc.top, rc.left + selStart.cx, rc.bottom);
				_TextOut(dc, rc_before, str_before.c_str(), str_before.length(), &pFonts->m_TextFont, m_style.text_color, txtFormat);
				x += selStart.cx + m_style.hilite_spacing;
			}
			{
				// zzz[yyy]
				std::wstring str_highlight(t.substr(range.start, range.end - range.start));
				CRect rc_hi(x, rc.top, x + (selEnd.cx - selStart.cx), rc.bottom);
				CRect rct = rc_hi;
				rc_hi.InflateRect(m_style.hilite_padding, m_style.hilite_padding);
				IsToRoundStruct rd = m_layout->GetTextRoundInfo();
				if (rc_hi.left > rc.left && rd.Hemispherical)
					rd.IsTopLeftNeedToRound = false;
				_HighlightText(dc, rc_hi, m_style.hilited_back_color, m_style.hilited_shadow_color,
					m_style.round_corner, BackType::TEXT, false, rd);
				_TextOut(dc, rct, str_highlight.c_str(), str_highlight.length(), &pFonts->m_TextFont, m_style.hilited_text_color, txtFormat);
				x += (selEnd.cx - selStart.cx);
			}
			if (range.end < static_cast<int>(t.length())) {
				// zzz[yyy]xxx
				x += m_style.hilite_spacing;
				std::wstring str_after(t.substr(range.end));
				CRect rc_after(x, rc.top, rc.right, rc.bottom);
				_TextOut(dc, rc_after, str_after.c_str(), str_after.length(), &pFonts->m_TextFont, m_style.text_color, txtFormat);

			}
		}
		else {
			CRect rcText(rc.left, rc.top, rc.right, rc.bottom);
			_TextOut(dc, rcText, t.c_str(), t.length(), &pFonts->m_TextFont, m_style.text_color, txtFormat);
		}
		drawn = true;
	}
	return drawn;
}

static inline BackType CalcBacktype(int index, int candsize)
{
	BackType bkType = BackType::FIRST_CAND;
	if (candsize == 1) bkType = BackType::ONLY_CAND;
	else if (index != 0 && index != candsize - 1) bkType = BackType::MID_CAND;
	else if (index == candsize - 1) bkType = BackType::LAST_CAND;
	return bkType;
}

bool WeaselPanel::_DrawCandidates(CDCHandle dc)
{
	bool drawn = false;
	const std::vector<Text> &candidates(m_ctx.cinfo.candies);
	const std::vector<Text> &comments(m_ctx.cinfo.comments);
	const std::vector<Text> &labels(m_ctx.cinfo.labels);

	int candidatesize = candidates.size();
	if(!candidatesize)	return drawn;

	BackType bkType = BackType::FIRST_CAND;
	IDWriteTextFormat1* txtFormat = (m_style.color_font) ? pDWR->pTextFormat : NULL;
	IDWriteTextFormat1* labeltxtFormat = (m_style.color_font) ? pDWR->pLabelTextFormat : NULL;
	IDWriteTextFormat1* commenttxtFormat = (m_style.color_font) ? pDWR->pCommentTextFormat : NULL;

	// if candidate_shadow_color not transparent, draw candidate shadow first
	if(COLORNOTTRANSPARENT(m_style.candidate_shadow_color))
		for (size_t i = 0; i < candidatesize && i < MAX_CANDIDATES_COUNT; ++i) {
			if (i == m_ctx.cinfo.highlighted) continue;	// draw non hilited candidates only 
			bkType = CalcBacktype(i, candidatesize);
			CRect rect = m_layout->GetCandidateRect((int)i);
			rect.InflateRect(m_style.hilite_padding, m_style.hilite_padding);
			_HighlightText(dc, rect, 0x00000000, m_style.candidate_shadow_color, m_style.round_corner, bkType);
			drawn = true;
		}
	// draw non highlighted candidates, without shadow
	for (size_t i = 0; i < candidatesize && i < MAX_CANDIDATES_COUNT; ++i) {
		CRect rect;
		if (COLORNOTTRANSPARENT(m_style.candidate_back_color))	// if transparent not to draw
		{
			if (i == m_ctx.cinfo.highlighted) continue;
			bkType = CalcBacktype(i, candidatesize);
			rect = m_layout->GetCandidateRect((int)i);
			rect.InflateRect(m_style.hilite_padding, m_style.hilite_padding);
			IsToRoundStruct rd = m_layout->GetRoundInfo(i);
			_HighlightText(dc, rect, m_style.candidate_back_color, 0x00000000, m_style.round_corner, bkType, false, rd);
		}
		// Draw label
		std::wstring label = m_layout->GetLabelText(labels, (int)i, m_style.label_text_format.c_str());
		if(!label.empty()) {
			rect = m_layout->GetCandidateLabelRect((int)i);
			_TextOut(dc, rect, label.c_str(), label.length(), &pFonts->m_LabelFont, m_style.label_text_color, labeltxtFormat);
		}
		// Draw text
		std::wstring text = candidates.at(i).str;
		if(!text.empty()) {
			rect = m_layout->GetCandidateTextRect((int)i);
			_TextOut(dc, rect, text.c_str(), text.length(), &pFonts->m_TextFont, m_style.candidate_text_color, txtFormat);
		}
		// Draw comment
		std::wstring comment = comments.at(i).str;
		if (!comment.empty()) {
			rect = m_layout->GetCandidateCommentRect((int)i);
			_TextOut(dc, rect, comment.c_str(), comment.length(), &pFonts->m_CommentFont, m_style.comment_text_color, commenttxtFormat);
		}
		drawn = true;
	}
	// draw highlighted candidate, on top of others
	{
		bkType = CalcBacktype(m_ctx.cinfo.highlighted, candidatesize);
		CRect rect = m_layout->GetHighlightRect();
		rect.InflateRect(m_style.hilite_padding, m_style.hilite_padding);
		IsToRoundStruct rd = m_layout->GetRoundInfo(m_ctx.cinfo.highlighted);
		_HighlightText(dc, rect, m_style.hilited_candidate_back_color, m_style.hilited_candidate_shadow_color, m_style.round_corner, bkType, true, rd);
		// Draw label
		std::wstring label = m_layout->GetLabelText(labels, m_ctx.cinfo.highlighted, m_style.label_text_format.c_str());
		if(!label.empty()) {
			rect = m_layout->GetCandidateLabelRect(m_ctx.cinfo.highlighted);
			_TextOut(dc, rect, label.c_str(), label.length(), &pFonts->m_LabelFont, m_style.hilited_label_text_color, labeltxtFormat);
		}

		// Draw text
		std::wstring text = candidates.at(m_ctx.cinfo.highlighted).str;
		if(!text.empty()) {
			rect = m_layout->GetCandidateTextRect((int)m_ctx.cinfo.highlighted);
			_TextOut(dc, rect, text.c_str(), text.length(), &pFonts->m_TextFont, m_style.hilited_candidate_text_color, txtFormat);
		}

		// Draw comment
		std::wstring comment = comments.at(m_ctx.cinfo.highlighted).str;
		if (!comment.empty()) {
			rect = m_layout->GetCandidateCommentRect((int)m_ctx.cinfo.highlighted);
			_TextOut(dc, rect, comment.c_str(), comment.length(), &pFonts->m_CommentFont, m_style.hilited_comment_text_color, commenttxtFormat);
		}
		drawn = true;
	}
	return drawn;
}

HBITMAP CopyDCToBitmap(HDC hDC, LPRECT lpRect)
{
	if (!hDC || !lpRect || IsRectEmpty(lpRect)) return NULL;
	HDC hMemDC;
	HBITMAP hBitmap, hOldBitmap;
	int nX, nY, nX2, nY2;
	int nWidth, nHeight;

	nX = lpRect->left;
	nY = lpRect->top;
	nX2 = lpRect->right;
	nY2 = lpRect->bottom;
	nWidth = nX2 - nX;
	nHeight = nY2 - nY;

	hMemDC = CreateCompatibleDC(hDC);
	hBitmap = CreateCompatibleBitmap(hDC, nWidth, nHeight);
	hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
	StretchBlt(hMemDC, 0, 0, nWidth, nHeight, hDC, nX, nY, nWidth, nHeight, SRCCOPY);
	hBitmap = (HBITMAP)SelectObject(hMemDC, hOldBitmap);

	DeleteDC(hMemDC);
	DeleteObject(hOldBitmap);
	return hBitmap;
 }

void WeaselPanel::_BlurBacktround(CRect& rc)
{

	if (setWindowCompositionAttribute != NULL 
		&& (((m_style.shadow_color & 0xff000000) == 0) || m_style.shadow_radius == 0) 
		&& (m_style.back_color >> 24) < 0xff
		&& (m_style.back_color >> 24) > 0
		&& _isWindows10OrGreater)
	{
		rc.DeflateRect(m_layout->offsetX, m_layout->offsetY);
		SetWindowRgn(CreateRoundRectRgn(rc.left, rc.top, rc.right + 1 + m_style.border, rc.bottom + 1 + m_style.border,
			m_style.round_corner_ex, m_style.round_corner_ex), true);
		setWindowCompositionAttribute(m_hWnd, &data);
	}
}
//draw client area
void WeaselPanel::DoPaint(CDCHandle dc)
{
	CRect rc;
	GetClientRect(&rc);
	// prepare memDC
	CDCHandle hdc = ::GetDC(m_hWnd);
	CDCHandle memDC = ::CreateCompatibleDC(hdc);
	HBITMAP memBitmap = ::CreateCompatibleBitmap(hdc, rc.Width(), rc.Height());
	::SelectObject(memDC, memBitmap);
	ReleaseDC(hdc);
	bool drawn = false;
	if (!hide_candidates) {
		CRect trc(rc);
		// background start
		if (!m_ctx.empty()) {
			CRect backrc = m_layout->GetContentRect();
			_HighlightText(memDC, backrc, m_style.back_color, m_style.shadow_color, m_style.round_corner_ex, BackType::BACKGROUND);
		}
		// background end
		// draw auxiliary string
		drawn |= _DrawPreedit(m_ctx.aux, memDC, m_layout->GetAuxiliaryRect());
		// draw preedit string
		if (!m_layout->IsInlinePreedit())
			drawn |= _DrawPreedit(m_ctx.preedit, memDC, m_layout->GetPreeditRect());
		// draw candidates
		drawn |= _DrawCandidates(memDC);
		// status icon (I guess Metro IME stole my idea :)
		if (m_layout->ShouldDisplayStatusIcon()) {
			if (!m_style.current_icon.empty())
				m_iconEnabled = (HICON)LoadImage(NULL, m_style.current_icon.c_str(), IMAGE_ICON, STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_LOADFROMFILE);
			else
				m_iconEnabled.LoadIconW(IDI_ZH, STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_DEFAULTCOLOR);
			CRect iconRect(m_layout->GetStatusIconRect());
			if(m_style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN 
					|| m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN)
				iconRect.OffsetRect(-1, -1);
			CIcon& icon(m_status.disabled ? m_iconDisabled : m_status.ascii_mode ? m_iconAlpha :
				((m_ctx.aux.str != L"全角" && m_ctx.aux.str != L"半角") ? m_iconEnabled : (m_status.full_shape ? m_iconFull : m_iconHalf)) );
			memDC.DrawIconEx(iconRect.left, iconRect.top, icon, 0, 0);
			drawn = true;
		}
		/* Nothing drawn, hide candidate window */
		if (!drawn)
			ShowWindow(SW_HIDE);
	}
	_LayerUpdate(rc, memDC);

	// blur_window swiching between enable and disable
	if (!m_style.blur_window) {
		accent.AccentState = ACCENT_DISABLED;
		SetWindowRgn(CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom), true);
		setWindowCompositionAttribute(m_hWnd, &data);
	}
	else
	{
		accent.AccentState = ACCENT_ENABLE_BLURBEHIND;
		_BlurBacktround(rc);
	}
	::DeleteDC(memDC);
	::DeleteObject(memBitmap);
}

void WeaselPanel::CaptureWindow()
{
	if (m_style.capture_type == UIStyle::CaptureType::NONE || hide_candidates)	return;
	HDC ScreenDC = ::GetDC(NULL);
	CRect rect;
	GetWindowRect(&rect);
	POINT WindowPosAtScreen = { rect.left, rect.top };
	if(m_style.capture_type == UIStyle::CaptureType::HIGHLIGHTED) {
		rect = m_layout->GetHighlightRect();
		rect.InflateRect(abs(m_style.margin_x), abs(m_style.margin_y));
		rect.OffsetRect(WindowPosAtScreen);
	}
	// capture input window
	if (OpenClipboard()) {
		HBITMAP bmp = CopyDCToBitmap(ScreenDC, LPRECT(rect));
		EmptyClipboard();
		SetClipboardData(CF_BITMAP, bmp);
		CloseClipboard();
		DeleteObject(bmp);
	}
	ReleaseDC(ScreenDC);
}

void WeaselPanel::_LayerUpdate(const CRect& rc, CDCHandle dc)
{
	HDC ScreenDC = ::GetDC(NULL);
	CRect rect;
	GetWindowRect(&rect);
	POINT WindowPosAtScreen = { rect.left, rect.top };
	POINT PointOriginal = { 0, 0 };
	SIZE sz = { rc.Width(), rc.Height() };

	BLENDFUNCTION bf = {AC_SRC_OVER, 0, 0XFF, AC_SRC_ALPHA};
	UpdateLayeredWindow(m_hWnd, ScreenDC, &WindowPosAtScreen, &sz, dc, &PointOriginal, RGB(0,0,0), &bf, ULW_ALPHA);
	ReleaseDC(ScreenDC);
}

LRESULT WeaselPanel::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	GetWindowRect(&m_inputPos);
	Refresh();
	return TRUE;
}

LRESULT WeaselPanel::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	return 0;
}

void WeaselPanel::MoveTo(RECT const& rc)
{
	m_inputPos = rc;
	if (m_style.shadow_offset_y >= 0)	m_inputPos.OffsetRect(0, 10);
	_RepositionWindow(true);
	RedrawWindow();
}

void WeaselPanel::_RepositionWindow(bool adj)
{
	RECT rcWorkArea;
	//SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
	memset(&rcWorkArea, 0, sizeof(rcWorkArea));
	HMONITOR hMonitor = MonitorFromRect(m_inputPos, MONITOR_DEFAULTTONEAREST);
	if (hMonitor) {
		MONITORINFO info;
		info.cbSize = sizeof(MONITORINFO);
		if (GetMonitorInfo(hMonitor, &info)) {
			rcWorkArea = info.rcWork;
		}
	}
	RECT rcWindow;
	GetWindowRect(&rcWindow);
	int width = (rcWindow.right - rcWindow.left);
	int height = (rcWindow.bottom - rcWindow.top);
	// keep panel visible
	rcWorkArea.right -= width;
	rcWorkArea.bottom -= height;
	int x = m_inputPos.left;
	int y = m_inputPos.bottom;
	if (m_style.shadow_radius > 0) {
		x -= (m_style.shadow_offset_x >= 0) ? m_layout->offsetX : (COLORNOTTRANSPARENT(m_style.shadow_color)? 0 : (m_style.margin_x - m_style.hilite_padding));
		if(adj)
		{
			y -= (m_style.shadow_offset_y >= 0) ? m_layout->offsetY : (COLORNOTTRANSPARENT(m_style.shadow_color)? 0 : (m_style.margin_y - m_style.hilite_padding));
			y -= m_style.shadow_radius / 2;
		}
	}
	if (x > rcWorkArea.right)
		x = rcWorkArea.right;
	if (x < rcWorkArea.left)
		x = rcWorkArea.left;
	// show panel above the input focus if we're around the bottom
	if (y > rcWorkArea.bottom)
		y = m_inputPos.top - height;
	if (y > rcWorkArea.bottom)
		y = rcWorkArea.bottom;
	if (y < rcWorkArea.top)
		y = rcWorkArea.top;
	// memorize adjusted position (to avoid window bouncing on height change)
	m_inputPos.bottom = y;
	if (m_oinputPos.left == x && m_oinputPos.bottom == y) return;
	m_oinputPos.left = x;
	m_oinputPos.bottom = y;
	SetWindowPos(HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

static inline void PreMutiplyBits(void* pvBits, const BITMAPINFOHEADER& BMIH, const COLORREF& inColor)
{
	BYTE* DataPtr = (BYTE*)pvBits;
	BYTE FillR = GetRValue(inColor);
	BYTE FillG = GetGValue(inColor);
	BYTE FillB = GetBValue(inColor);
	BYTE ThisA;
	for (int LoopY = 0; LoopY < BMIH.biHeight; LoopY++) {
		for (int LoopX = 0; LoopX < BMIH.biWidth; LoopX++) {
			ThisA = *DataPtr; // move alpha and premutiply with rgb
			*DataPtr++ = (FillB * ThisA) >> 8;
			*DataPtr++ = (FillG * ThisA) >> 8;
			*DataPtr++ = (FillR * ThisA) >> 8;
			*DataPtr++ = ThisA;
		}
	}
}

static HRESULT _CreateAlphaTextBitmapEx(CRect rc, LPCWSTR inText, const CFont& inFont, COLORREF inColor, size_t cch, HBITMAP& MyBMP, bool fallback = true)
{
    HRESULT hr;
    SCRIPT_STRING_ANALYSIS ssa;
	HDC hTextDC = CreateCompatibleDC(NULL);
	HFONT hOldFont = (HFONT)SelectObject(hTextDC, inFont);
	RECT TextArea = { 0, 0, 0, 0 };
	int width, height;
	if (fallback) {
		width = rc.Width();
		height = rc.Height();
		hr = ScriptStringAnalyse(
			hTextDC,
			inText, cch,
			2 * cch + 16,
			-1,
			SSA_GLYPHS | SSA_FALLBACK | SSA_LINK,
			0,
			NULL, // control
			NULL, // state
			NULL, // piDx
			NULL,
			NULL, // pbInClass
			&ssa);
	}
	else {
		DrawText(hTextDC, inText, cch, &TextArea, DT_CALCRECT);		/* get text area */
		width = TextArea.right - TextArea.left;
		height = TextArea.bottom - TextArea.top;
	}
	if (width > 0 && height > 0) {
		void* pvBits = NULL;
		BITMAPINFOHEADER BMIH = PREPAREBITMAPINFOHEADER(width, height);
		// create and select dib into dc
		MyBMP = CreateDIBSection(hTextDC, (LPBITMAPINFO)&BMIH, 0, (LPVOID*)&pvBits, NULL, 0);
		HBITMAP hOldBMP = (HBITMAP)SelectObject(hTextDC, MyBMP);
		if (hOldBMP != NULL) {
			SetTextColor(hTextDC, 0x00FFFFFF);
			SetBkColor(hTextDC, 0x00000000);
			SetBkMode(hTextDC, OPAQUE);
			// draw text to buffer
			if (fallback && SUCCEEDED(hr))
				hr = ScriptStringOut(ssa, 0, 0, 0, rc, 0, 0, FALSE);
			else
				DrawText(hTextDC, inText, cch, &TextArea, DT_NOCLIP);
			PreMutiplyBits(pvBits, BMIH, inColor);
			SelectObject(hTextDC, hOldBMP);
		}
	}
	SelectObject(hTextDC, hOldFont);
	DeleteDC(hTextDC);
	if (fallback) { 
		hr = ScriptStringFree(&ssa);
		return hr;
	}
	return S_OK;
}

static void _AlphaBlendBmpToDC(CDCHandle& dc, const int x, const int y, const BYTE alpha, const HBITMAP& MyBMP)
{
	HDC hTempDC = CreateCompatibleDC(dc);
	HBITMAP hOldBMP = (HBITMAP)SelectObject(hTempDC, MyBMP);
	if (hOldBMP) {
		BITMAP BMInf;
		GetObject(MyBMP, sizeof(BITMAP), &BMInf);
		// fill blend function and blend new text to window
		BLENDFUNCTION bf = { AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
		AlphaBlend(dc, x, y, BMInf.bmWidth, BMInf.bmHeight, hTempDC, 0, 0, BMInf.bmWidth, BMInf.bmHeight, bf);
		// clean up
		SelectObject(hTempDC, hOldBMP);
		DeleteDC(hTempDC);
	}
}

static HRESULT _TextOutWithFallback(CDCHandle dc, int x, int y, CRect const rc, LPCWSTR psz, int cch, const CFont& font, const COLORREF& inColor, BYTE alpha)
{
	HRESULT hr;
	HBITMAP MyBMP = NULL;
	hr = _CreateAlphaTextBitmapEx(rc, psz, font, inColor, cch, MyBMP, true);
	if (MyBMP && SUCCEEDED(hr)) {
		_AlphaBlendBmpToDC(dc, x, y, alpha, MyBMP);
		DeleteObject(MyBMP);
	}
	return hr;
}

bool WeaselPanel::_TextOutWithFallbackDW (CDCHandle dc, CRect const rc, std::wstring psz, size_t cch, COLORREF gdiColor, IDWriteTextFormat* pTextFormat)
{
	float r = (float)(GetRValue(gdiColor))/255.0f;
	float g = (float)(GetGValue(gdiColor))/255.0f;
	float b = (float)(GetBValue(gdiColor))/255.0f;
	float alpha = (float)((gdiColor >> 24) & 255) / 255.0f;
	pBrush->SetColor(D2D1::ColorF(r, g, b, alpha));

	if (NULL != pBrush && NULL != pDWR->pTextFormat) {
		pDWR->pDWFactory->CreateTextLayout( psz.c_str(), (UINT32)psz.size(), pTextFormat, (float)rc.Width(), (float)rc.Height(), &pDWR->pTextLayout);
		// offsetx for font glyph over left
		float offsetx = 0.0f;
		float offsety = 0.0f;
		DWRITE_OVERHANG_METRICS omt;
		pDWR->pTextLayout->GetOverhangMetrics(&omt);
		if (omt.left > 0)
			offsetx += omt.left;
		pDWR->pRenderTarget->BindDC(dc, &rc);
		pDWR->pRenderTarget->BeginDraw();
		if (pDWR->pTextLayout != NULL)
			pDWR->pRenderTarget->DrawTextLayout({ offsetx, offsety}, pDWR->pTextLayout, pBrush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
		// for rect checking
		//D2D1_RECT_F rectf{ 0,0, rc.Width(), rc.Height() };
		//pDWR->pRenderTarget->DrawRectangle(&rectf, pBrush);
		pDWR->pRenderTarget->EndDraw();
		SafeRelease(&pDWR->pTextLayout);
	}
	else
		return false;
	return true;
}

void WeaselPanel::_TextOut(CDCHandle dc, CRect const& rc, LPCWSTR psz, size_t cch, FontInfo* pFontInfo, int inColor, IDWriteTextFormat* pTextFormat)
{
	if (m_style.color_font ) {
		if(pTextFormat == NULL) return;
		_TextOutWithFallbackDW(dc, rc, psz, cch, inColor, pTextFormat);
	}
	else { 
		if(pFontInfo->m_FontPoint <= 0)	return;
		CFont font;
		CSize size;
		long height = -MulDiv(pFontInfo->m_FontPoint, dc.GetDeviceCaps(LOGPIXELSY), 72);
		dc.SetTextColor(inColor & 0x00ffffff);
		font.CreateFontW(height, 0, 0, 0, pFontInfo->m_FontWeight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, pFontInfo->m_FontFace.c_str());
		dc.SelectFont(font);
		BYTE alpha = (inColor >> 24) & 255 ;
		int offset = 0;
		// split strings with \r, for multiline string drawing
		std::vector<std::wstring> lines;
		boost::algorithm::split(lines, psz, boost::algorithm::is_any_of(L"\r"));
		for (auto line : lines) {
			// calc line size, for y offset calc
			dc.GetTextExtent(line.c_str(), (int)line.length(), &size);
			if (FAILED(_TextOutWithFallback(dc, rc.left, rc.top + offset, rc, line.c_str(), (int)line.length(), font, inColor, alpha))) {
				HBITMAP MyBMP = NULL;
				_CreateAlphaTextBitmapEx(rc, psz, font, dc.GetTextColor(), cch, MyBMP, false);
				if (MyBMP) {
					_AlphaBlendBmpToDC(dc, rc.left, rc.top + offset, alpha, MyBMP);
					delete MyBMP;
				}
			}
			offset += size.cy;
		}
	}
}

