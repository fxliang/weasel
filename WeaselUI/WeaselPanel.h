#pragma once
#include <WeaselCommon.h>
#include <WeaselUI.h>
#include <Usp10.h>
#include "Layout.h"
#include "GdiplusBlur.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

using namespace weasel;

typedef CWinTraits<WS_POPUP|WS_CLIPSIBLINGS|WS_DISABLED, WS_EX_TOOLWINDOW|WS_EX_TOPMOST> CWeaselPanelTraits;

enum class BackType
{
	TEXT = 0,
	FIRST_CAND = 1,
	MID_CAND = 2,
	LAST_CAND = 3,
	ONLY_CAND = 4,
	BACKGROUND = 5	// background
};

class WeaselPanel : 
	public CWindowImpl<WeaselPanel, CWindow, CWeaselPanelTraits>,
	CDoubleBufferImpl<WeaselPanel>
{
public:
	BEGIN_MSG_MAP(WeaselPanel)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(CDoubleBufferImpl<WeaselPanel>)
	END_MSG_MAP()

	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	WeaselPanel(weasel::UI &ui);
	~WeaselPanel();

	void MoveTo(RECT const& rc);
	void Refresh();
	void InitFontRes(void);
	void DoPaint(CDCHandle dc);
	void CleanUp();
	void CaptureWindow();
private:
	void _CreateLayout();
	void _ResizeWindow();
	void _RepositionWindow(bool adj = false);
	bool _DrawPreedit(weasel::Text const& text, CDCHandle dc, CRect const& rc);
	bool _DrawCandidates(CDCHandle dc);
	void _HighlightText(CDCHandle dc, CRect rc, COLORREF color, COLORREF shadowColor, int radius, BackType type, bool highlighted, 
		IsToRoundStruct rd);
	void _TextOut(CDCHandle dc, CRect const& rc, LPCWSTR psz, size_t cch, FontInfo* pFontInfo, int inColor, IDWriteTextFormat* pTextFormat = NULL);
	bool _TextOutWithFallbackDW(CDCHandle dc, CRect const rc, std::wstring psz, size_t cch, COLORREF gdiColor, IDWriteTextFormat* pTextFormat);

	void _LayerUpdate(const CRect& rc, CDCHandle dc);

	weasel::Layout *m_layout;
	weasel::Context &m_ctx;
	weasel::Context &m_octx;
	weasel::Status &m_status;
	weasel::UIStyle &m_style;
	weasel::UIStyle &m_ostyle;

	CRect m_inputPos;
	CRect m_oinputPos;
	CSize m_osize;

	CIcon m_iconDisabled;
	CIcon m_iconEnabled;
	CIcon m_iconAlpha;
	CIcon m_iconFull;
	CIcon m_iconHalf;
	// for gdiplus drawings
	Gdiplus::GdiplusStartupInput _m_gdiplusStartupInput;
	ULONG_PTR _m_gdiplusToken;
	// for hemispherical dome
	CRect bgRc;
	BYTE m_candidateCount;

	bool hide_candidates;
	// for multi font_face & font_point
	GdiplusBlur* m_blurer;
	DirectWriteResources* pDWR;
	GDIFonts* pFonts;
	ID2D1SolidColorBrush* pBrush;
};

