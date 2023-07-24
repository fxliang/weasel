#include "stdafx.h"
#include <StringAlgorithm.hpp>
#include <WeaselUtility.h>

#include "Deserializer.h"
#include "ContextUpdater.h"

using namespace weasel;
namespace weasel {
	void to_json(json& j, const std::wstring& w) {
		j = wstring_to_string(w, CP_UTF8);
	}
	void from_json(const json& j, std::wstring& w) {
		w = string_to_wstring(j.get<std::string>(), CP_UTF8);
	}
	void to_json(json& j, const TextAttributeType& tat) {
		switch (tat) {
		case TextAttributeType::NONE:
			j = "none";
			break;
		case TextAttributeType::HIGHLIGHTED:
			j = "highlighted";
			break;
		case TextAttributeType::LAST_TYPE:
			j = "last_type";
			break;
		}
	}
	void from_json(const json& j, TextAttributeType& tat) {
		if (j == "none")
			tat = TextAttributeType::NONE;
		else if (j == "highlighted")
			tat = TextAttributeType::HIGHLIGHTED;
		else if(j == "last_type")
			tat = TextAttributeType::LAST_TYPE;
	}
	// auto serialize and deserialize TextRange
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TextRange, start, end, cursor)
	// auto serialize and deserialize TextAttribute
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TextAttribute, range, type)
	// auto serialize and deserialize Text
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Text, str, attributes)
	// auto serialize and deserialize CandidateInfo
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CandidateInfo, currentPage, is_last_page, totalPages, highlighted, candies, comments, labels)
	// auto serialize and deserialize Context
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Context, preedit, aux, cinfo)
};
// ContextUpdater

Deserializer::Ptr ContextUpdater::Create(ResponseParser* pTarget)
{
	return Deserializer::Ptr(new ContextUpdater(pTarget));
}

ContextUpdater::ContextUpdater(ResponseParser* pTarget)
	: Deserializer(pTarget)
{
}

ContextUpdater::~ContextUpdater()
{
}

void ContextUpdater::Store(Deserializer::KeyType const& k, std::wstring const& value)
{
	if(!m_pTarget->p_context || k.size() < 2)
		return;

	if (k[1] == L"preedit")
	{
		_StoreText(m_pTarget->p_context->preedit, k, value);
		return;
	}

	if (k[1] == L"aux")
	{
		_StoreText(m_pTarget->p_context->aux, k, value);
		return;
	}

	if (k[1] == L"cand")
	{
		_StoreCand(k, value);
		return;
	}
}

void ContextUpdater::_StoreText(Text& target, Deserializer::KeyType k, std::wstring const& value)
{
	if(k.size() == 2)
	{
		target.clear();
		target.str = value;
		return;
	}
	if(k.size() == 3)
	{
		// ctx.preedit.cursor
		if (k[2] == L"cursor")
		{
			std::vector<std::wstring> vec;
			split(vec, value, L",");
			if (vec.size() < 2)
				return;

			weasel::TextAttribute attr;
			attr.type = HIGHLIGHTED;
			attr.range.start = _wtoi(vec[0].c_str());
			attr.range.end = _wtoi(vec[1].c_str());
			attr.range.cursor = _wtoi(vec[2].c_str());
			if (attr.range.cursor < attr.range.start || attr.range.cursor > attr.range.end)
			{
				attr.range.cursor = attr.range.end;
			}
			
			target.attributes.push_back(attr);
			return;
		}
	}
}

void ContextUpdater::_StoreCand(Deserializer::KeyType k, std::wstring const& value)
{
	CandidateInfo& cinfo = m_pTarget->p_context->cinfo;
	json j = json::parse(value);
	j.get_to(cinfo);
}

// StatusUpdater

Deserializer::Ptr StatusUpdater::Create(ResponseParser* pTarget)
{
	return Deserializer::Ptr(new StatusUpdater(pTarget));
}

StatusUpdater::StatusUpdater(ResponseParser* pTarget)
: Deserializer(pTarget)
{
}

StatusUpdater::~StatusUpdater()
{
}

void StatusUpdater::Store(Deserializer::KeyType const& k, std::wstring const& value)
{
	if(!m_pTarget->p_status || k.size() < 2)
		return;

	bool bool_value = (!value.empty() && value != L"0");

	if (k[1] == L"schema_id")
	{
		m_pTarget->p_status->schema_id = value;
		return;
	}

	if (k[1] == L"ascii_mode")
	{
		m_pTarget->p_status->ascii_mode = bool_value;
		return;
	}

	if (k[1] == L"composing")
	{
		m_pTarget->p_status->composing = bool_value;
		return;
	}

	if (k[1] == L"disabled")
	{
		m_pTarget->p_status->disabled = bool_value;
		return;
	}

	if (k[1] == L"full_shape")
	{
		m_pTarget->p_status->full_shape = bool_value;
		return;
	}
}
