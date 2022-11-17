#include "stdafx.h"
#include <string>
#include "fontClasses.h"

DirectWriteResources::DirectWriteResources(const weasel::UIStyle& style) :
	dpiScaleX_(0),
	dpiScaleY_(0),
	pD2d1Factory(NULL),
	pDWFactory(NULL),
	pRenderTarget(NULL),
	pTextFormat(NULL),
	pLabelTextFormat(NULL),
	pCommentTextFormat(NULL)
{
	// prepare d2d1 resources
	HRESULT hResult = S_OK;
	// create factory
	if (pD2d1Factory == NULL)
		hResult = ::D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &pD2d1Factory);
	// create IDWriteFactory
	if (pDWFactory == NULL)
		hResult = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWFactory));
	/* ID2D1HwndRenderTarget */
	if (pRenderTarget == NULL)
	{
		const D2D1_PIXEL_FORMAT format = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
		const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, format);
		pD2d1Factory->CreateDCRenderTarget(&properties, &pRenderTarget);
		pRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
		pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	}
	//get the dpi information
	pD2d1Factory->GetDesktopDpi(&dpiScaleX_, &dpiScaleY_);
	dpiScaleX_ /= 72.0f;
	dpiScaleY_ /= 72.0f;

	InitResources(style);
}

DirectWriteResources::~DirectWriteResources()
{
	SafeRelease(&pTextFormat);
	SafeRelease(&pLabelTextFormat);
	SafeRelease(&pCommentTextFormat);
	SafeRelease(&pRenderTarget);
	SafeRelease(&pDWFactory);
	SafeRelease(&pD2d1Factory);
}

HRESULT DirectWriteResources::InitResources(std::wstring label_font_face, int label_font_point,
	std::wstring font_face, int font_point,
	std::wstring comment_font_face, int comment_font_point) 
{
	// prepare d2d1 resources

	HRESULT hResult = S_OK;
	std::vector<std::wstring> fontFaceStrVector;
	// text font text format set up
	boost::algorithm::split(fontFaceStrVector, font_face, boost::algorithm::is_any_of(L","));
	std::wstring mainFontFace;
	DWRITE_FONT_WEIGHT fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
	DWRITE_FONT_STYLE fontStyle = DWRITE_FONT_STYLE_NORMAL;
	_ParseFontFace(fontFaceStrVector[0], mainFontFace, fontWeight, fontStyle, TextFontInfo);
	TextFontInfo.m_FontPoint = font_point;
	hResult = pDWFactory->CreateTextFormat(mainFontFace.c_str(), NULL,
			fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL,
			font_point * dpiScaleX_, L"", reinterpret_cast<IDWriteTextFormat**>(&pTextFormat));
	if( pTextFormat != NULL)
	{
		pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
		pTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		if (fontFaceStrVector.size() > 1)
			_SetFontFallback(pTextFormat, fontFaceStrVector);
	}
	fontFaceStrVector.swap(std::vector<std::wstring>());

	// label font text format set up
	boost::algorithm::split(fontFaceStrVector, label_font_face, boost::algorithm::is_any_of(L","));
	fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
	fontStyle = DWRITE_FONT_STYLE_NORMAL;
	_ParseFontFace(fontFaceStrVector[0], mainFontFace, fontWeight, fontStyle, LabelTextFontInfo);
	LabelTextFontInfo.m_FontPoint = label_font_point;
	hResult = pDWFactory->CreateTextFormat(mainFontFace.c_str(), NULL,
			fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL,
			label_font_point * dpiScaleX_, L"", reinterpret_cast<IDWriteTextFormat**>(&pLabelTextFormat));
	if( pLabelTextFormat != NULL)
	{
		pLabelTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		pLabelTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
		pLabelTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		if (fontFaceStrVector.size() > 1)
			_SetFontFallback(pLabelTextFormat, fontFaceStrVector);
	}
	fontFaceStrVector.swap(std::vector<std::wstring>());

	// comment font text format set up
	boost::algorithm::split(fontFaceStrVector, comment_font_face, boost::algorithm::is_any_of(L","));
	fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
	fontStyle = DWRITE_FONT_STYLE_NORMAL;
	_ParseFontFace(fontFaceStrVector[0], mainFontFace, fontWeight, fontStyle, CommentTextFontInfo);
	CommentTextFontInfo.m_FontPoint = comment_font_point;
	hResult = pDWFactory->CreateTextFormat(mainFontFace.c_str(), NULL,
			fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL,
			comment_font_point * dpiScaleX_, L"", reinterpret_cast<IDWriteTextFormat**>(&pCommentTextFormat));
	if( pCommentTextFormat != NULL)
	{
		pCommentTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		pCommentTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
		pCommentTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		if (fontFaceStrVector.size() > 1)
			_SetFontFallback(pCommentTextFormat, fontFaceStrVector);
	}
	fontFaceStrVector.swap(std::vector<std::wstring>());
	return hResult;
}

HRESULT DirectWriteResources::InitResources(const UIStyle& style)
{
	// prepare d2d1 resources
	HRESULT hResult = S_OK;

	std::vector<std::wstring> fontFaceStrVector;
	// text font text format set up
	boost::algorithm::split(fontFaceStrVector, style.font_face, boost::algorithm::is_any_of(L","));
	std::wstring mainFontFace;
	DWRITE_FONT_WEIGHT fontWeight = DWRITE_FONT_WEIGHT_NORMAL;

	DWRITE_FONT_STYLE fontStyle = DWRITE_FONT_STYLE_NORMAL;
	_ParseFontFace(fontFaceStrVector[0], mainFontFace, fontWeight, fontStyle, LabelTextFontInfo);
	hResult = pDWFactory->CreateTextFormat(mainFontFace.c_str(), NULL,
			fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL,
			style.font_point * dpiScaleX_, L"", reinterpret_cast<IDWriteTextFormat**>(&pTextFormat));
	if( pTextFormat != NULL)
	{
		pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		// candidate text always center vertical
		pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
		pTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		if (fontFaceStrVector.size() > 1)
			_SetFontFallback(pTextFormat, fontFaceStrVector);
	}
	fontFaceStrVector.swap(std::vector<std::wstring>());

	// label font text format set up
	boost::algorithm::split(fontFaceStrVector, style.label_font_face, boost::algorithm::is_any_of(L","));
	fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
	fontStyle = DWRITE_FONT_STYLE_NORMAL;
	_ParseFontFace(fontFaceStrVector[0], mainFontFace, fontWeight, fontStyle, TextFontInfo);
	hResult = pDWFactory->CreateTextFormat(mainFontFace.c_str(), NULL,
			fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL,
			style.label_font_point * dpiScaleX_, L"", reinterpret_cast<IDWriteTextFormat**>(&pLabelTextFormat));
	if( pLabelTextFormat != NULL)
	{
		pLabelTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		pLabelTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
		pLabelTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		if (fontFaceStrVector.size() > 1)
			_SetFontFallback(pLabelTextFormat, fontFaceStrVector);
	}
	fontFaceStrVector.swap(std::vector<std::wstring>());

	// comment font text format set up
	boost::algorithm::split(fontFaceStrVector, style.comment_font_face, boost::algorithm::is_any_of(L","));
	fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
	fontStyle = DWRITE_FONT_STYLE_NORMAL;
	_ParseFontFace(fontFaceStrVector[0], mainFontFace, fontWeight, fontStyle, CommentTextFontInfo);
	hResult = pDWFactory->CreateTextFormat(mainFontFace.c_str(), NULL,
			fontWeight, fontStyle, DWRITE_FONT_STRETCH_NORMAL,
			style.comment_font_point * dpiScaleX_, L"", reinterpret_cast<IDWriteTextFormat**>(&pCommentTextFormat));
	if( pCommentTextFormat != NULL)
	{
		pCommentTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		pCommentTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
		pCommentTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		if (fontFaceStrVector.size() > 1)
			_SetFontFallback(pCommentTextFormat, fontFaceStrVector);
	}
	fontFaceStrVector.swap(std::vector<std::wstring>());

	return hResult;
}

void DirectWriteResources::_ParseFontFace(const std::wstring fontFaceStr,
		std::wstring& fontFace, DWRITE_FONT_WEIGHT& fontWeight, DWRITE_FONT_STYLE& fontStyle,
		FontInfo& fontInfo)
{
	fontInfo.m_FontFace = fontFaceStr;
	std::vector<std::wstring> parsedStrV; 
	boost::algorithm::split(parsedStrV, fontFaceStr, boost::algorithm::is_any_of(L":"));
	fontFace = parsedStrV[0];

	if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":thin", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_THIN;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":extra_light", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_EXTRA_LIGHT;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":ultra_light", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_ULTRA_LIGHT;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":light", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_LIGHT;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":semi_light", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_SEMI_LIGHT;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":medium", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_MEDIUM;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":demi_bold", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_DEMI_BOLD;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":semi_bold", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":bold", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_BOLD;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":extra_bold", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_EXTRA_BOLD;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":ultra_bold", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_ULTRA_BOLD;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":black", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_BLACK;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":heavy", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_HEAVY;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":extra_black", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_EXTRA_BLACK;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":ultra_black", boost::wregex::icase)))
		fontWeight = DWRITE_FONT_WEIGHT_ULTRA_BLACK;
	else
		fontWeight = DWRITE_FONT_WEIGHT_NORMAL;

	if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":italic", boost::wregex::icase)))
		fontStyle = DWRITE_FONT_STYLE_ITALIC;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":oblique", boost::wregex::icase)))
		fontStyle = DWRITE_FONT_STYLE_OBLIQUE;
	else
		fontStyle = DWRITE_FONT_STYLE_NORMAL;
}

void DirectWriteResources::_SetFontFallback(IDWriteTextFormat1* pTextFormat, std::vector<std::wstring> fontVector)
{
	IDWriteFontFallback* pSysFallback;
	pDWFactory->GetSystemFontFallback(&pSysFallback);
	IDWriteFontFallback* pFontFallback = NULL;
	IDWriteFontFallbackBuilder* pFontFallbackBuilder = NULL;
	pDWFactory->CreateFontFallbackBuilder(&pFontFallbackBuilder);
	std::vector<std::wstring> fallbackFontsVector;
	for (UINT32 i = 1; i < fontVector.size(); i++)
	{
		boost::algorithm::split(fallbackFontsVector, fontVector[i], boost::algorithm::is_any_of(L":"));
		std::wstring _fontFaceWstr, firstWstr, lastWstr;
		if (fallbackFontsVector.size() == 3)
		{
			_fontFaceWstr = fallbackFontsVector[0];
			firstWstr = fallbackFontsVector[1];
			lastWstr = fallbackFontsVector[2];
			if (lastWstr.empty())
				lastWstr = L"10ffff";
			if (firstWstr.empty())
				firstWstr = L"0";
		}
		else if (fallbackFontsVector.size() == 2)	// fontName : codepoint
		{
			_fontFaceWstr = fallbackFontsVector[0];
			firstWstr = fallbackFontsVector[1];
			if (firstWstr.empty())
				firstWstr = L"0";
			lastWstr = L"10ffff";
		}
		else if (fallbackFontsVector.size() == 1)	// if only font defined, use all range
		{
			_fontFaceWstr = fallbackFontsVector[0];
			firstWstr = L"0";
			lastWstr = L"10ffff";
		}
		UINT first = 0, last = 0x10ffff;
		try {
			first = std::stoi(firstWstr.c_str(), 0, 16);
		}
		catch(...){
			first = 0;
		}
		try {
			last = std::stoi(lastWstr.c_str(), 0, 16);
		}
		catch(...){
			first = 0x10ffff;
		}
		DWRITE_UNICODE_RANGE range = { first, last };
		const  WCHAR* familys = { _fontFaceWstr.c_str() };
		pFontFallbackBuilder->AddMapping(&range, 1, &familys, 1);
		fallbackFontsVector.swap(std::vector<std::wstring>());
	}
	pFontFallbackBuilder->AddMappings(pSysFallback);
	pFontFallbackBuilder->CreateFontFallback(&pFontFallback);
	pTextFormat->SetFontFallback(pFontFallback);
	SafeRelease(&pFontFallback);
	SafeRelease(&pSysFallback);
	SafeRelease(&pFontFallbackBuilder);
}


GDIFonts::GDIFonts(const UIStyle& style) 
{
	std::vector<std::wstring> fontFaceStrVector;
	boost::algorithm::split(fontFaceStrVector, style.label_font_face, boost::algorithm::is_any_of(L","));
	_ParseFontFace(fontFaceStrVector[0], m_LabelFont.m_FontFace, m_LabelFont.m_FontWeight);
	m_LabelFont.m_FontPoint = style.label_font_point;
	fontFaceStrVector.swap(std::vector<std::wstring>());

	boost::algorithm::split(fontFaceStrVector, style.font_face, boost::algorithm::is_any_of(L","));
	_ParseFontFace(fontFaceStrVector[0], m_TextFont.m_FontFace, m_TextFont.m_FontWeight);
	m_TextFont.m_FontPoint = style.font_point;
	fontFaceStrVector.swap(std::vector<std::wstring>());

	boost::algorithm::split(fontFaceStrVector, style.comment_font_face, boost::algorithm::is_any_of(L","));
	_ParseFontFace(fontFaceStrVector[0], m_CommentFont.m_FontFace, m_CommentFont.m_FontWeight);
	m_CommentFont.m_FontPoint = style.comment_font_point;
	fontFaceStrVector.swap(std::vector<std::wstring>());
}

void GDIFonts::_ParseFontFace(const std::wstring fontFaceStr, std::wstring& fontFace, int& fontWeight)
{
	std::vector<std::wstring> parsedStrV; 
	boost::algorithm::split(parsedStrV, fontFaceStr, boost::algorithm::is_any_of(L":"));
	fontFace = parsedStrV[0];
	if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":thin", boost::wregex::icase)))
		fontWeight = FW_THIN;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":(extra_light|ultra_light)", boost::wregex::icase)))
		fontWeight = FW_EXTRALIGHT;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":(light|semi_light|demi_light)", boost::wregex::icase)))
		fontWeight = FW_LIGHT;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":medium", boost::wregex::icase)))
		fontWeight = FW_MEDIUM;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":bold", boost::wregex::icase)))
		fontWeight = FW_BOLD;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":(semi_bold|demi_bold)", boost::wregex::icase)))
		fontWeight = FW_SEMIBOLD;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":(extra_bold|ultra_bold)", boost::wregex::icase)))
		fontWeight = FW_EXTRABOLD;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":(extra_black|ultra_black|heavy|black)", boost::wregex::icase)))
		fontWeight = FW_BLACK;
	else if (boost::regex_search(fontFaceStr, boost::wsmatch(), boost::wregex(L":normal", boost::wregex::icase)))
		fontWeight = FW_NORMAL;
	else
		fontWeight = FW_DONTCARE;
}
