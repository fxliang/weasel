#include "stdafx.h"
#include "Deserializer.h"
#include "Styler.h"

using namespace weasel;

namespace weasel {

	void to_json(json& j, const UIStyle::AntiAliasMode& aam) {
		switch(aam) {
			case UIStyle::DEFAULT:
				j = "default";
				break;
			case UIStyle::CLEARTYPE:
				j = "cleartype";
				break;
			case UIStyle::GRAYSCALE:
				j = "grayscale";
				break;
			case UIStyle::ALIASED:
				j = "aliased";
				break;
			case UIStyle::FORCE_DWORD:
				j = "force_dword";
				break;
		}
	}
	void from_json(const json& j, UIStyle::AntiAliasMode& aam) {
		if(j == "default") aam = UIStyle::DEFAULT;
		else if(j == "cleartype") aam = UIStyle::CLEARTYPE;
		else if(j == "grayscale") aam = UIStyle::GRAYSCALE;
		else if(j == "aliased") aam = UIStyle::ALIASED;
		else if(j == "force_dword") aam = UIStyle::FORCE_DWORD;
	}
	void to_json(json& j, const UIStyle::LayoutAlignType& lat) {
		switch(lat){
			case UIStyle::ALIGN_TOP:
				j = "top";
				break;
			case UIStyle::ALIGN_CENTER:
				j = "center";
				break;
			case UIStyle::ALIGN_BOTTOM:
				j = "bottom";
				break;
		}
	}
	void from_json(const json& j, UIStyle::LayoutAlignType& lat) {
		if(j == "top") lat = UIStyle::ALIGN_TOP;
		else if(j == "center") lat = UIStyle::ALIGN_CENTER;
		else if(j == "bottom") lat = UIStyle::ALIGN_BOTTOM;
	}
	void to_json(json& j, const UIStyle::LayoutType& lt) {
		switch(lt){
			case UIStyle::LAYOUT_VERTICAL:
				j = "vertical";
				break;
			case UIStyle::LAYOUT_HORIZONTAL:
				j = "horizontal";
				break;
			case UIStyle::LAYOUT_VERTICAL_TEXT:
				j = "vertical_text";
				break;
			case UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN:
				j = "horizontal+fullscreen";
				break;
			case UIStyle::LAYOUT_VERTICAL_FULLSCREEN:
				j = "vertical+fullscreen";
				break;
			case UIStyle::LAYOUT_TYPE_LAST:
				j = "last_type";
				break;
		}
	}
	void from_json(const json& j, UIStyle::LayoutType& lt) {
		if(j == "vertical")		lt = UIStyle::LAYOUT_VERTICAL;
		else if(j == "horizontal")	lt = UIStyle::LAYOUT_HORIZONTAL;
		else if(j == "vertical_text")	lt = UIStyle::LAYOUT_VERTICAL_TEXT;
		else if(j == "horizontal+fullscreen")	lt = UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN;
		else if(j == "vertical+fullscreen")		lt = UIStyle::LAYOUT_VERTICAL_FULLSCREEN;
		else if(j == "last_type")	lt = UIStyle::LAYOUT_TYPE_LAST;
	}
	void to_json(json& j, const UIStyle::PreeditType& pt) {
		switch(pt){
			case UIStyle::COMPOSITION:
				j = "composition";
				break;
			case UIStyle::PREVIEW:
				j = "preview";
				break;
			case UIStyle::PREVIEW_ALL:
				j = "preview_all";
				break;
		}
	}
	void from_json(const json& j, UIStyle::PreeditType& pt) {
		if(j == "composition")	pt = UIStyle::COMPOSITION;
		else if(j == "preview") pt = UIStyle::PREVIEW;
		else if(j == "preview_all") pt = UIStyle::PREVIEW_ALL;
	}
	// auto serialize and deserialize ColorScheme
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ColorScheme, \
		text_color, candidate_text_color, candidate_back_color,\
		candidate_shadow_color, candidate_border_color, label_text_color,\
		comment_text_color, back_color, shadow_color,\
		border_color, hilited_text_color, hilited_back_color,\
		hilited_shadow_color, hilited_candidate_text_color, hilited_candidate_back_color,\
		hilited_candidate_shadow_color, hilited_candidate_border_color, hilited_label_text_color,\
		hilited_comment_text_color, hilited_mark_color, prevpage_color, nextpage_color)

	// auto serialize and deserialize UIStyle struct
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UIStyle, antialias_mode, preedit_type, paging_on_scroll, mouse_hover_ms,\
		font_face, label_font_face, comment_font_face, font_point,\
		label_font_point, comment_font_point, inline_preedit, display_tray_icon,\
		ascii_tip_follow_cursor, current_zhung_icon, current_ascii_icon, current_half_icon,\
		current_full_icon, enhanced_position, label_text_format, mark_text,\
		layout_type, align_type, vertical_text_left_to_right, vertical_text_with_wrap,\
		min_width, max_width, min_height, max_height,\
		border, margin_x, margin_y, spacing,\
		candidate_spacing, hilite_spacing, hilite_padding_x, hilite_padding_y,\
		round_corner, round_corner_ex, shadow_radius, shadow_offset_x,\
		shadow_offset_y, vertical_auto_reverse, color_scheme, client_caps)
};

Styler::Styler(weasel::ResponseParser * pTarget)
	: Deserializer(pTarget)
{
}

Styler::~Styler()
{
}

void Styler::Store(weasel::Deserializer::KeyType const & key, std::wstring const & value)
{
	if (!m_pTarget->p_style) return;

	UIStyle &sty = *m_pTarget->p_style;

	json j = json::parse(value);
	j.get_to(sty);
}

weasel::Deserializer::Ptr Styler::Create(weasel::ResponseParser * pTarget)
{
	return Deserializer::Ptr(new Styler(pTarget));
}
