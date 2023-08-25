#include "stdafx.h"
#include "Deserializer.h"
#include "Styler.h"

using namespace weasel;

namespace weasel {

	DEFINE_ENUM_JSON_SERIALIZATION(UIStyle::AntiAliasMode)
	DEFINE_ENUM_JSON_SERIALIZATION(UIStyle::PreeditType)
	DEFINE_ENUM_JSON_SERIALIZATION(UIStyle::LayoutType)
	DEFINE_ENUM_JSON_SERIALIZATION(UIStyle::LayoutAlignType)

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
		current_full_icon, enhanced_position, click_to_capture, label_text_format, mark_text,\
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
