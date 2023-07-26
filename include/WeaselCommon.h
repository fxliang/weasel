#pragma once

#include <json.hpp>
using json = nlohmann::json;

#define WEASEL_IME_NAME L"小狼毫"
#define WEASEL_REG_KEY L"Software\\Rime\\Weasel"
#define RIME_REG_KEY L"Software\\Rime"

//#define USE_SHARP_COLOR_CODE

//#define _DEBUG_
namespace weasel
{

#define DEFINE_ENUM_JSON_SERIALIZATION(EnumType) \
    void to_json(json& j, const EnumType& value) \
    { \
        j = static_cast<int>(value); \
    } \
    void from_json(const json& j, EnumType& value) \
    { \
        value = static_cast<EnumType>(j.get<int>()); \
    }
// macro for easily define extern prototype 
#define EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(TypeName) \
	extern void to_json(json& j, const TypeName& var);\
	extern void from_json(const json& j, TypeName& var);

	enum TextAttributeType
	{
		NONE = 0,
		HIGHLIGHTED,
		LAST_TYPE
	};

	struct TextRange
	{
		TextRange() : start(0), end(0), cursor(-1) {}
		TextRange(int _start, int _end, int _cursor) : start(_start), end(_end), cursor(_cursor) {}
		bool operator==(const TextRange& tr)
		{
			return (start == tr.start && end == tr.end && cursor == tr.cursor);
		}
		bool operator!=(const TextRange& tr)
		{
			return (start != tr.start || end != tr.end || cursor != tr.cursor);
		}
		int start;
		int end;
		int cursor;
	};

	struct TextAttribute
	{
		TextAttribute() : type(NONE) {}
		TextAttribute(int _start, int _end, TextAttributeType _type) : range(_start, _end, -1), type(_type) {}
		bool operator==(const TextAttribute& ta)
		{
			return (range == ta.range && type == ta.type);
		}
		bool operator!=(const TextAttribute& ta)
		{
			return (range != ta.range || type != ta.type);
		}
		TextRange range;
		TextAttributeType type;
	};

	struct Text
	{
		Text() : str(L"") {}
		Text(std::wstring const& _str) : str(_str) {}
		void clear()
		{
			str.clear();
			attributes.clear();
		}
		bool empty() const
		{
			return str.empty();
		}
		bool operator==(const Text& txt)
		{
			if (str != txt.str || (attributes.size() != txt.attributes.size()))
				return false;
			for (size_t i = 0; i < attributes.size(); i++)
			{
				if ((attributes[i] != txt.attributes[i]))
					return false;
			}
			return true;
		}
		bool operator!=(const Text& txt)
		{
			if (str != txt.str || (attributes.size() != txt.attributes.size()))
				return true;
			for (size_t i = 0; i < attributes.size(); i++)
			{
				if ((attributes[i] != txt.attributes[i]))
					return true;
			}
			return false;
		}
		std::wstring str;
		std::vector<TextAttribute> attributes;
	};

	struct CandidateInfo
	{
		CandidateInfo()
		{
			currentPage = 0;
			totalPages = 0;
			highlighted = 0;
			is_last_page = false;
		}
		void clear()
		{
			currentPage = 0;
			totalPages = 0;
			highlighted = 0;
			is_last_page = false;
			candies.clear();
			labels.clear();
		}
		bool empty() const
		{
			return candies.empty();
		}
		bool operator==(const CandidateInfo& ci)
		{
			if (currentPage != ci.currentPage
				|| totalPages != ci.totalPages
				|| highlighted != ci.highlighted
				|| is_last_page != ci.is_last_page
				|| notequal(candies, ci.candies)
				|| notequal(comments, ci.comments)
				|| notequal(labels, ci.labels)
				)
				return false;
			return true;
		}
		bool operator!=(const CandidateInfo& ci)
		{
			if (currentPage != ci.currentPage
				|| totalPages != ci.totalPages
				|| highlighted != ci.highlighted
				|| is_last_page != ci.is_last_page
				|| notequal(candies, ci.candies)
				|| notequal(comments, ci.comments)
				|| notequal(labels, ci.labels)
				)
				return true;
			return false;
		}
		bool notequal(std::vector<Text> txtSrc, std::vector<Text> txtDst)
		{
			if (txtSrc.size() != txtDst.size()) return true;
			for (size_t i = 0; i < txtSrc.size(); i++)
			{
				if (txtSrc[i] != txtDst[i])
					return true;
			}
			return false;
		}
		int currentPage;
		bool is_last_page;
		int totalPages;
		int highlighted;
		std::vector<Text> candies;
		std::vector<Text> comments;
		std::vector<Text> labels;
	};

	struct Context
	{
		Context() {}
		void clear()
		{
			preedit.clear();
			aux.clear();
			cinfo.clear();
		}
		bool empty() const
		{
			return preedit.empty() && aux.empty() && cinfo.empty();
		}
		bool operator==(const Context& ctx)
		{
			if (preedit == ctx.preedit && aux == ctx.aux || cinfo == ctx.cinfo)
				return true;
			return false;
		}
		bool operator!=(const Context& ctx)
		{
			return !(operator==(ctx));
		}

		bool operator!()
		{
			if (preedit.str.empty()
				&& aux.str.empty()
				&& cinfo.candies.empty()
				&& cinfo.labels.empty()
				&& cinfo.comments.empty())
				return true;
			else
				return false;
		}
		Text preedit;
		Text aux;
		CandidateInfo cinfo;
	};

	// 由ime管理
	struct Status
	{
		Status() : ascii_mode(false), composing(false), disabled(false), full_shape(false) {}
		void reset()
		{
			schema_name.clear();
			schema_id.clear();
			ascii_mode = false;
			composing = false;
			disabled = false;
			full_shape = false;
		}
		// 輸入方案
		std::wstring schema_name;
		// 輸入方案 id
		std::wstring schema_id;
		// 轉換開關
		bool ascii_mode;
		// 寫作狀態
		bool composing;
		// 維護模式（暫停輸入功能）
		bool disabled;
		// 全角状态
		bool full_shape;
	};

	// 用於向前端告知設置信息
	struct Config
	{
		Config() : inline_preedit(false) {}
		void reset()
		{
			inline_preedit = false;
		}
		bool inline_preedit;
	};

	struct ColorScheme
	{
		// color scheme
		int text_color;
		int candidate_text_color;
		int candidate_back_color;
		int candidate_shadow_color;
		int candidate_border_color;
		int label_text_color;
		int comment_text_color;
		int back_color;
		int shadow_color;
		int border_color;
		int hilited_text_color;
		int hilited_back_color;
		int hilited_shadow_color;
		int hilited_candidate_text_color;
		int hilited_candidate_back_color;
		int hilited_candidate_shadow_color;
		int hilited_candidate_border_color;
		int hilited_label_text_color;
		int hilited_comment_text_color;
		int hilited_mark_color;
		int prevpage_color;
		int nextpage_color;

		ColorScheme() : text_color(0),
		candidate_text_color(0),
		candidate_back_color(0),
		candidate_shadow_color(0),
		candidate_border_color(0),
		label_text_color(0),
		comment_text_color(0),
		back_color(0),
		shadow_color(0),
		border_color(0),
		hilited_text_color(0),
		hilited_back_color(0),
		hilited_shadow_color(0),
		hilited_candidate_text_color(0),
		hilited_candidate_back_color(0),
		hilited_candidate_shadow_color(0),
		hilited_candidate_border_color(0),
		hilited_label_text_color(0),
		hilited_comment_text_color(0),
		hilited_mark_color(0),
		prevpage_color(0),
		nextpage_color(0) {}
		bool operator!=(const ColorScheme& color){
			return (
					text_color != color.text_color
					||candidate_text_color != color.candidate_text_color
					||candidate_back_color != color.candidate_back_color
					||candidate_shadow_color != color.candidate_shadow_color
					||candidate_border_color != color.candidate_border_color
					||label_text_color != color.label_text_color
					||comment_text_color != color.comment_text_color
					||back_color != color.back_color
					||shadow_color != color.shadow_color
					||border_color != color.border_color
					||hilited_text_color != color.hilited_text_color
					||hilited_back_color != color.hilited_back_color
					||hilited_shadow_color != color.hilited_shadow_color
					||hilited_candidate_text_color != color.hilited_candidate_text_color
					||hilited_candidate_back_color != color.hilited_candidate_back_color
					||hilited_candidate_shadow_color != color.hilited_candidate_shadow_color
					||hilited_candidate_border_color != color.hilited_candidate_border_color
					||hilited_label_text_color != color.hilited_label_text_color
					||hilited_comment_text_color != color.hilited_comment_text_color
					||hilited_mark_color != color.hilited_mark_color
					||prevpage_color != color.prevpage_color
					||nextpage_color != color.nextpage_color
					);
		}
	};

	struct UIStyle
	{
		enum AntiAliasMode
		{
			DEFAULT = 0,
			CLEARTYPE = 1,
			GRAYSCALE = 2,
			ALIASED = 3,
			FORCE_DWORD = 0xffffffff
		};

		enum PreeditType
		{
			COMPOSITION,
			PREVIEW,
			PREVIEW_ALL
		};

		enum LayoutType
		{
			LAYOUT_VERTICAL = 0,
			LAYOUT_HORIZONTAL,
			LAYOUT_VERTICAL_TEXT,
			LAYOUT_VERTICAL_FULLSCREEN,
			LAYOUT_HORIZONTAL_FULLSCREEN,
			LAYOUT_TYPE_LAST
		};

		enum LayoutAlignType
		{
			ALIGN_BOTTOM = 0,
			ALIGN_CENTER,
			ALIGN_TOP
		};

		// font face and font point settings
		std::wstring font_face;
		std::wstring label_font_face;
		std::wstring comment_font_face;
		int font_point;
		int label_font_point;
		int comment_font_point;

		bool inline_preedit;
		bool display_tray_icon;
		bool ascii_tip_follow_cursor;
		bool paging_on_scroll;
		bool enhanced_position;
		int mouse_hover_ms;
		AntiAliasMode antialias_mode;
		PreeditType preedit_type;
		// custom icon settings
		std::wstring current_zhung_icon;
		std::wstring current_ascii_icon;
		std::wstring current_half_icon;
		std::wstring current_full_icon;
		// label format and mark_text
		std::wstring label_text_format;
		std::wstring mark_text;
		// layout relative parameters
		LayoutType layout_type;
		LayoutAlignType align_type;
		bool vertical_text_left_to_right;
		bool vertical_text_with_wrap;
		// layout, with key name like style/layout/...
		int min_width;
		int max_width;
		int min_height;
		int max_height;
		int border;
		int margin_x;
		int margin_y;
		int spacing;
		int candidate_spacing;
		int hilite_spacing;
		int hilite_padding_x;
		int hilite_padding_y;
		int round_corner;
		int round_corner_ex;
		int shadow_radius;
		int shadow_offset_x;
		int shadow_offset_y;
		bool vertical_auto_reverse;
		// color scheme
		ColorScheme color_scheme;
		// per client
		int client_caps;

		UIStyle() : 
			font_face(),
			label_font_face(),
			comment_font_face(),
			font_point(0),
			label_font_point(0),
			comment_font_point(0),
			inline_preedit(false),
			display_tray_icon(false),
			ascii_tip_follow_cursor(false),
			paging_on_scroll(false),
			enhanced_position(false),
			mouse_hover_ms(0),
			antialias_mode(DEFAULT),
			preedit_type(COMPOSITION),
			current_zhung_icon(),
			current_ascii_icon(),
			current_half_icon(),
			current_full_icon(),
			label_text_format(L"%s."),
			mark_text(),
			layout_type(LAYOUT_VERTICAL),
			align_type(ALIGN_BOTTOM),
			vertical_text_left_to_right(false),
			vertical_text_with_wrap(false),
			min_width(0),
			max_width(0),
			min_height(0),
			max_height(0),
			border(0),
			margin_x(0),
			margin_y(0),
			spacing(0),
			candidate_spacing(0),
			hilite_spacing(0),
			hilite_padding_x(0),
			hilite_padding_y(0),
			round_corner(0),
			round_corner_ex(0),
			shadow_radius(0),
			shadow_offset_x(0),
			shadow_offset_y(0),
			vertical_auto_reverse(false),
			color_scheme(),
			client_caps(0) {}
		bool operator!=(const UIStyle& st)
		{
			return
				(
					align_type != st.align_type
					|| antialias_mode != st.antialias_mode
					|| preedit_type != st.preedit_type
					|| layout_type != st.layout_type
					|| vertical_text_left_to_right != st.vertical_text_left_to_right
					|| vertical_text_with_wrap != st.vertical_text_with_wrap
					|| paging_on_scroll != st.paging_on_scroll
					|| font_face != st.font_face
					|| label_font_face != st.label_font_face
					|| comment_font_face != st.comment_font_face
					|| mouse_hover_ms != st.mouse_hover_ms
					|| font_point != st.font_point
					|| label_font_point != st.label_font_point
					|| comment_font_point != st.comment_font_point
					|| inline_preedit != st.inline_preedit
					|| mark_text != st.mark_text
					|| display_tray_icon != st.display_tray_icon
					|| ascii_tip_follow_cursor != st.ascii_tip_follow_cursor
					|| current_zhung_icon != st.current_zhung_icon
					|| current_ascii_icon != st.current_ascii_icon
					|| current_half_icon != st.current_half_icon
					|| current_full_icon != st.current_full_icon
					|| enhanced_position != st.enhanced_position
					|| label_text_format != st.label_text_format
					|| min_width != st.min_width
					|| max_width != st.max_width
					|| min_height != st.min_height
					|| max_height != st.max_height
					|| border != st.border
					|| margin_x != st.margin_x
					|| margin_y != st.margin_y
					|| spacing != st.spacing
					|| candidate_spacing != st.candidate_spacing
					|| hilite_spacing != st.hilite_spacing
					|| hilite_padding_x != st.hilite_padding_x
					|| hilite_padding_y != st.hilite_padding_y
					|| round_corner != st.round_corner
					|| round_corner_ex != st.round_corner_ex
					|| shadow_radius != st.shadow_radius
					|| shadow_offset_x != st.shadow_offset_x
					|| shadow_offset_y != st.shadow_offset_y
					|| vertical_auto_reverse != st.vertical_auto_reverse
					|| color_scheme != st.color_scheme
					);
		}
	};

	// UIStyle vs json
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(UIStyle::AntiAliasMode)
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(UIStyle::LayoutAlignType)
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(UIStyle::LayoutType)
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(UIStyle::PreeditType)
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(ColorScheme)
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(UIStyle)
	// Context vs json
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(std::wstring)
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(TextAttributeType)
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(TextAttribute)
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(TextRange)
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(Text)
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(CandidateInfo)
	EXTERN_PROTOTYPE_SERIALIZE_DESERIALIZE(Context)
}
