﻿#include "stdafx.h"
#include <logging.h>
#include <RimeWithWeasel.h>
#include <StringAlgorithm.hpp>
#include <WeaselUtility.h>
#include <WeaselVersion.h>
#include <VersionHelpers.hpp>
#include <math.h>
#include <regex>
#include <rime_api.h>

static inline BOOL IsThemeLight()
{
	// only for windows 10 or greater, return false when lower version.
	OSVERSIONINFOEXW ovi = { sizeof ovi };
	GetVersionEx2((LPOSVERSIONINFOW)&ovi);
	if (ovi.dwMajorVersion < 10) return false;
	HKEY hKL;
	LPCWSTR addr = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
	//LPCWSTR key = L"SystemUsesLightTheme"; 
	LPCWSTR key = L"AppsUseLightTheme";
	DWORD dwType;
	BYTE values[16];
	DWORD dataLen;
	LSTATUS ret = RegOpenKeyEx(HKEY_CURRENT_USER, addr, 0, KEY_READ, &hKL);
	if (ret == ERROR_SUCCESS)
	{
		ret = RegQueryValueExW(hKL, key, 0, &dwType, values, &dataLen);
		RegCloseKey(hKL);
		return (values[0] != 0);
	}
	MessageBox(0, L"open reg failed, return false", L"", 0);
	return FALSE;
}

int expand_ibus_modifier(int m)
{
	return (m & 0xff) | ((m & 0xff00) << 16);
}

RimeWithWeaselHandler::RimeWithWeaselHandler(weasel::UI *ui)
	: m_ui(ui)
	, m_active_session(0)
	, m_disabled(true)
	, _UpdateUICallback(NULL)
	, m_vista_greater(IsWindowsVistaOrGreater())
{
	_Setup();
}

RimeWithWeaselHandler::~RimeWithWeaselHandler()
{
}

void _UpdateUIStyle(RimeConfig* config, weasel::UI* ui, bool initialize);
bool _UpdateUIStyleColor(RimeConfig* config, weasel::UIStyle& style, bool is_light);
void CopyColorScheme(weasel::UIStyle& style, weasel::UIStyle src);
void _LoadAppOptions(RimeConfig* config, AppOptionsByAppName& app_options);

void RimeWithWeaselHandler::_Setup()
{
	RIME_STRUCT(RimeTraits, weasel_traits);
	weasel_traits.shared_data_dir = weasel_shared_data_dir();
	weasel_traits.user_data_dir = weasel_user_data_dir();
	const int len = 20;
	char utf8_str[len];
	memset(utf8_str, 0, sizeof(utf8_str));
	WideCharToMultiByte(CP_UTF8, 0, WEASEL_IME_NAME, -1, utf8_str, len - 1, NULL, NULL);
	weasel_traits.distribution_name = utf8_str;
	weasel_traits.distribution_code_name = WEASEL_CODE_NAME;
	weasel_traits.distribution_version = WEASEL_VERSION;
	weasel_traits.app_name = "rime.weasel";
	RimeSetup(&weasel_traits);
	RimeSetNotificationHandler(&RimeWithWeaselHandler::OnNotify, this);
}

void RimeWithWeaselHandler::Initialize()
{
	m_disabled = _IsDeployerRunning();
	if (m_disabled)
	{
		 return;
	}

	LOG(INFO) << "Initializing la rime.";
	RimeInitialize(NULL);
	if (RimeStartMaintenance(/*full_check = */False))
	{
		m_disabled = true;
	}

	RimeConfig config = { NULL };
	if (RimeConfigOpen("weasel", &config))
	{
		if (m_ui)
		{
			bool is_light = IsThemeLight();
			_UpdateUIStyle(&config, m_ui, true);
			m_base_style = m_ui->style();
			m_base_style_dark = m_base_style;
			_UpdateUIStyleColor(&config, m_base_style, true);	// light theme
			if (!_UpdateUIStyleColor(&config, m_base_style_dark, false))	// dark theme
				m_base_style_dark = m_base_style;
			m_ui->style() = is_light ? m_base_style : m_base_style_dark;
		}
		_LoadAppOptions(&config, m_app_options);
		RimeConfigClose(&config);
	}
	m_last_schema_id.clear();
}

void RimeWithWeaselHandler::Finalize()
{
	m_active_session = 0;
	m_disabled = true;
	LOG(INFO) << "Finalizing la rime.";
	RimeFinalize();
}

UINT RimeWithWeaselHandler::FindSession(UINT session_id)
{
	if (m_disabled) return 0;
	Bool found = RimeFindSession(session_id);
	DLOG(INFO) << "Find session: session_id = " << session_id << ", found = " << found;
	return found ? session_id : 0;
}

UINT RimeWithWeaselHandler::AddSession(LPWSTR buffer, EatLine eat)
{
	if (m_disabled)
	{
		DLOG(INFO) << "Trying to resume service.";
		EndMaintenance();
		if (m_disabled) return 0;
	}
	UINT session_id = RimeCreateSession();
	DLOG(INFO) << "Add session: created session_id = " << session_id;
	_ReadClientInfo(session_id, buffer);

	if (m_ui)
	{
		if (IsThemeLight())
			CopyColorScheme(m_ui->style(), m_base_style);
		else
			CopyColorScheme(m_ui->style(), m_base_style_dark);
	}
	// show session's welcome message :-) if any
	if (eat) {
		_Respond(session_id, eat);
	}
	_UpdateUI(session_id);
	m_active_session = session_id;
	return session_id;
}

UINT RimeWithWeaselHandler::RemoveSession(UINT session_id)
{
	if (m_ui) m_ui->Hide();
	if (m_disabled) return 0;
	DLOG(INFO) << "Remove session: session_id = " << session_id;
	// TODO: force committing? otherwise current composition would be lost
	RimeDestroySession(session_id);
	m_active_session = 0;
	return 0;
}

namespace ibus
{
	enum Keycode
	{
		Escape = 0xFF1B,
	};
}

BOOL RimeWithWeaselHandler::ProcessKeyEvent(weasel::KeyEvent keyEvent, UINT session_id, EatLine eat)
{
	DLOG(INFO) << "Process key event: keycode = " << keyEvent.keycode << ", mask = " << keyEvent.mask
		 << ", session_id = " << session_id;
	if (m_disabled) return FALSE;
	Bool handled = RimeProcessKey(session_id, keyEvent.keycode, expand_ibus_modifier(keyEvent.mask));
	_Respond(session_id, eat);
	_UpdateUI(session_id);
	m_active_session = session_id;
	return (BOOL)handled;
}

void RimeWithWeaselHandler::CommitComposition(UINT session_id)
{
	DLOG(INFO) << "Commit composition: session_id = " << session_id;
	if (m_disabled) return;
	RimeCommitComposition(session_id);
	_UpdateUI(session_id);
	m_active_session = session_id;
}

void RimeWithWeaselHandler::ClearComposition(UINT session_id)
{
	DLOG(INFO) << "Clear composition: session_id = " << session_id;
	if (m_disabled) return;
	RimeClearComposition(session_id);
	_UpdateUI(session_id);
	m_active_session = session_id;
}

void RimeWithWeaselHandler::FocusIn(DWORD client_caps, UINT session_id)
{
	DLOG(INFO) << "Focus in: session_id = " << session_id << ", client_caps = " << client_caps;
	if (m_disabled) return;
	_UpdateUI(session_id);
	m_active_session = session_id;
}

void RimeWithWeaselHandler::FocusOut(DWORD param, UINT session_id)
{
	DLOG(INFO) << "Focus out: session_id = " << session_id;
	if (m_ui) m_ui->Hide();
	m_active_session = 0;
}

void RimeWithWeaselHandler::UpdateInputPosition(RECT const& rc, UINT session_id)
{
	DLOG(INFO) << "Update input position: (" << rc.left << ", " << rc.top
		<< "), session_id = " << session_id << ", m_active_session = " << m_active_session;
	if (m_ui) m_ui->UpdateInputPosition(rc);
	if (m_disabled) return;
	if (m_active_session != session_id)
	{
		_UpdateUI(session_id);
		m_active_session = session_id;
	}
}

std::string RimeWithWeaselHandler::m_message_type;
std::string RimeWithWeaselHandler::m_message_value;

void RimeWithWeaselHandler::OnNotify(void* context_object,
	                                 uintptr_t session_id,
                                     const char* message_type,
                                     const char* message_value)
{
	// may be running in a thread when deploying rime
	RimeWithWeaselHandler* self = reinterpret_cast<RimeWithWeaselHandler*>(context_object);
	if (!self || !message_type || !message_value) return;
	m_message_type = message_type;
	m_message_value = message_value;
}

void RimeWithWeaselHandler::_ReadClientInfo(UINT session_id, LPWSTR buffer)
{
	std::string app_name;
	std::string client_type;
	// parse request text
	wbufferstream bs(buffer, WEASEL_IPC_BUFFER_LENGTH);
	std::wstring line;
	while (bs.good())
	{
		std::getline(bs, line);
		if (!bs.good())
			break;
		// file ends
		if (line == L".")
			break;
		const std::wstring kClientAppKey = L"session.client_app=";
		if (starts_with(line, kClientAppKey))
		{
			std::wstring lwr = line;
			to_lower(lwr);
			app_name = wcstoutf8(lwr.substr(kClientAppKey.length()).c_str());
		}
		const std::wstring kClientTypeKey = L"session.client_type=";
		if (starts_with(line, kClientTypeKey))
		{
			client_type = wcstoutf8(line.substr(kClientTypeKey.length()).c_str());
		}
	}
    // set app specific options
	if (!app_name.empty())
	{
		RimeSetProperty(session_id, "client_app", app_name.c_str());

		if (m_app_options.find(app_name) != m_app_options.end())
		{
			AppOptions& options(m_app_options[app_name]);
			std::for_each(options.begin(), options.end(), [session_id](std::pair<const std::string, bool> &pair)
			{
				DLOG(INFO) << "set app option: " << pair.first << " = " << pair.second;
				RimeSetOption(session_id, pair.first.c_str(), Bool(pair.second));
			});
		}
	}
	// ime | tsf
	RimeSetProperty(session_id, "client_type", client_type.c_str());
	// inline preedit
	bool inline_preedit = m_ui->style().inline_preedit && (client_type == "tsf");	
	RimeSetOption(session_id, "inline_preedit", Bool(inline_preedit));
	// show soft cursor on weasel panel but not inline
	RimeSetOption(session_id, "soft_cursor", Bool(!inline_preedit));
}

std::wstring ConvertCRLF(std::wstring strString, std::wstring strCRLF)
{
	std::wstring strRet;
	std::wstring::iterator ite = strString.begin();
	std::wstring::iterator iteEnd = strString.end();
	if (0 < strString.size()) {
		wchar_t wNextChar = *ite++;
		while (1) {
			if ('\r' == wNextChar) {
				strRet += strCRLF;
				if (ite == iteEnd) { break; }
				wNextChar = *ite++;
				if ('\n' == wNextChar) {
					if (ite == iteEnd) { break; }
					wNextChar = *ite++;
				}
			}
			else if ('\n' == wNextChar) {
				strRet += strCRLF;
				if (ite == iteEnd) { break; }
				wNextChar = *ite++;
				if ('\r' == wNextChar) {
					if (ite == iteEnd) { break; }
					wNextChar = *ite++;
				}
			}
			else {
				strRet += wNextChar;
				if (ite == iteEnd) { break; }
				wNextChar = *ite++;
			}
		};
	}
	return(strRet);
}

void RimeWithWeaselHandler::_GetCandidateInfo(weasel::CandidateInfo & cinfo, RimeContext & ctx)
{
	cinfo.candies.resize(ctx.menu.num_candidates);
	cinfo.comments.resize(ctx.menu.num_candidates);
	cinfo.labels.resize(ctx.menu.num_candidates);
	for (int i = 0; i < ctx.menu.num_candidates; ++i)
	{
		cinfo.candies[i].str = ConvertCRLF(utf8towcs(ctx.menu.candidates[i].text), L"\r");
		if (ctx.menu.candidates[i].comment)
		{
			cinfo.comments[i].str = ConvertCRLF(utf8towcs(ctx.menu.candidates[i].comment), L"\r");
		}
		if (RIME_STRUCT_HAS_MEMBER(ctx, ctx.select_labels) && ctx.select_labels)
		{
			cinfo.labels[i].str = ConvertCRLF(utf8towcs(ctx.select_labels[i]), L"\r");
		}
		else if (ctx.menu.select_keys)
		{
			cinfo.labels[i].str = ConvertCRLF(std::wstring(1, ctx.menu.select_keys[i]), L"\r");
		}
		else
		{
			cinfo.labels[i].str = std::to_wstring((i + 1) % 10);
		}
	}
	cinfo.highlighted = ctx.menu.highlighted_candidate_index;
	cinfo.currentPage = ctx.menu.page_no;
}

void RimeWithWeaselHandler::StartMaintenance()
{
	Finalize();
	_UpdateUI(0);
}

void RimeWithWeaselHandler::EndMaintenance()
{
	if (m_disabled)
	{
		Initialize();
		_UpdateUI(0);
	}
}

void RimeWithWeaselHandler::SetOption(UINT session_id, const std::string & opt, bool val)
{
	RimeSetOption(session_id, opt.c_str(), val);
}

void RimeWithWeaselHandler::OnUpdateUI(std::function<void()> const &cb)
{
	_UpdateUICallback = cb;
}

bool RimeWithWeaselHandler::_IsDeployerRunning()
{
	HANDLE hMutex = CreateMutex(NULL, TRUE, L"WeaselDeployerMutex");
	bool deployer_detected = hMutex && GetLastError() == ERROR_ALREADY_EXISTS;
	if (hMutex)
	{
		CloseHandle(hMutex);
	}
	return deployer_detected;
}

void RimeWithWeaselHandler::_UpdateUI(UINT session_id)
{
	weasel::Status weasel_status;
	weasel::Context weasel_context;

	bool is_tsf = _IsSessionTSF(session_id);

	if (session_id == 0)
		weasel_status.disabled = m_disabled;

	_GetStatus(weasel_status, session_id);

	if (!is_tsf) {
		_GetContext(weasel_context, session_id);
	}

	if (!m_ui) return;
	if (RimeGetOption(session_id, "inline_preedit"))
		m_ui->style().client_caps |= weasel::INLINE_PREEDIT_CAPABLE;
	else
		m_ui->style().client_caps &= ~weasel::INLINE_PREEDIT_CAPABLE;


	if (weasel_status.composing)
	{
		m_ui->Update(weasel_context, weasel_status);
		if (!is_tsf) m_ui->Show();
	}
	else if (!_ShowMessage(weasel_context, weasel_status))
	{
		m_ui->Hide();
		m_ui->Update(weasel_context, weasel_status);
	}
	
	// Dangerous, don't touch
	static char app_name[50];
	RimeGetProperty(session_id, "client_app", app_name, sizeof(app_name) - 1);
	if (utf8towcs(app_name) == std::wstring(L"explorer.exe") && m_vista_greater) {
		boost::thread th([=]() {
			::Sleep(100);
			if (_UpdateUICallback) _UpdateUICallback();
		});
	}
	else {
		if (_UpdateUICallback) _UpdateUICallback();
	}

	m_message_type.clear();
	m_message_value.clear();
}

void RimeWithWeaselHandler::_LoadSchemaSpecificSettings(const std::string& schema_id)
{
	if (!m_ui) return;
	RimeConfig config;
	if (!RimeSchemaOpen(schema_id.c_str(), &config))
		return;
	bool is_light = IsThemeLight();
	RimeConfig configw = { NULL };
	if (RimeConfigOpen("weasel", &configw))
	{
		if (m_ui)
		{
			_UpdateUIStyle(&configw, m_ui, true);
			if (is_light)
				m_base_style = m_ui->style();
			else
				m_base_style_dark = m_ui->style();
		}
		RimeConfigClose(&configw);
	}
	//m_ui->style() = m_base_style;
	_UpdateUIStyle(&config, m_ui, false);
	const int BUF_SIZE = 2047;
	char buffer[BUF_SIZE + 1];
	memset(buffer, '\0', sizeof(buffer));
	if (RimeConfigGetString(&config, "schema/icon", buffer, BUF_SIZE))
	{
		std::wstring tmp = utf8towcs(buffer);
		std::wstring user_dir = utf8towcs(weasel_user_data_dir());
		DWORD dwAttrib = GetFileAttributes((user_dir + L"\/" + tmp).c_str());
		if (!(INVALID_FILE_ATTRIBUTES != dwAttrib && 0 == (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)))
		{
			std::wstring share_dir = utf8towcs(weasel_shared_data_dir());
			dwAttrib = GetFileAttributes((share_dir + L"\/" + tmp).c_str());
			if (!(INVALID_FILE_ATTRIBUTES != dwAttrib && 0 == (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)))
				m_ui->style().current_icon = L"";
			else
				m_ui->style().current_icon = (share_dir + L"\/" + tmp);
		}
		else
			m_ui->style().current_icon = user_dir + L"\/" + tmp;
	}
	else
		m_ui->style().current_icon = L"";
	RimeConfigClose(&config);
	if (is_light)
		CopyColorScheme(m_ui->style(), m_base_style);
	else
		CopyColorScheme(m_ui->style(), m_base_style_dark);
}

bool RimeWithWeaselHandler::_ShowMessage(weasel::Context& ctx, weasel::Status& status) {
	// show as auxiliary string
	std::wstring& tips(ctx.aux.str);
	bool show_icon = false;
	if (m_message_type == "deploy") {
		if (m_message_value == "start")
			tips = L"正在部署 RIME";
		else if (m_message_value == "success")
			tips = L"部署完成";
		else if (m_message_value == "failure")
			tips = L"有錯誤，請查看日誌 %TEMP%\\rime.weasel.*.INFO";
	}
	else if (m_message_type == "schema") {
		tips = /*L"【" + */status.schema_name/* + L"】"*/;
	}
	else if (m_message_type == "option") {
		if (m_message_value == "!ascii_mode")
		{
			show_icon = true;  
		}
		else if (m_message_value == "ascii_mode")
		{
			show_icon = true;  
		}
		else if (m_message_value == "!full_shape")
			tips = L"半角";
		else if (m_message_value == "full_shape")
			tips = L"全角";
		else if (m_message_value == "!ascii_punct")
			tips = L"，。";
		else if (m_message_value == "ascii_punct")
			tips = L"，．";
		else if (m_message_value == "!simplification")
			tips = L"漢字";
		else if (m_message_value == "simplification")
			tips = L"汉字";
	}
	if (tips.empty() && !show_icon)
		return m_ui->IsCountingDown();

	m_ui->Update(ctx, status);
	m_ui->ShowWithTimeout(1200 + 200 * tips.length());
	return true;
}
inline std::string _GetLabelText(const std::vector<weasel::Text> &labels, int id, const wchar_t *format)
{
	wchar_t buffer[128];
	swprintf_s<128>(buffer, format, labels.at(id).str.c_str());
	return to_byte_string(std::wstring(buffer));
}

bool RimeWithWeaselHandler::_Respond(UINT session_id, EatLine eat)
{
	std::set<std::string> actions;
	std::list<std::string> messages;

	// extract information

	RIME_STRUCT(RimeCommit, commit);
	if (RimeGetCommit(session_id, &commit))
	{
		actions.insert("commit");
		messages.push_back(std::string("commit=") + commit.text + '\n');
		RimeFreeCommit(&commit);
	}
	
	bool is_composing = false;
	RIME_STRUCT(RimeStatus, status);
	if (RimeGetStatus(session_id, &status))
	{
		is_composing = !!status.is_composing;
		actions.insert("status");
		messages.push_back(std::string("status.ascii_mode=") + std::to_string(status.is_ascii_mode) + '\n');
		messages.push_back(std::string("status.composing=") + std::to_string(status.is_composing) + '\n');
		messages.push_back(std::string("status.disabled=") + std::to_string(status.is_disabled) + '\n');
		RimeFreeStatus(&status);
	}
	
	RIME_STRUCT(RimeContext, ctx);
	if (RimeGetContext(session_id, &ctx))
	{
		if (is_composing)
		{
			actions.insert("ctx");
			switch (m_ui->style().preedit_type)
			{
			case weasel::UIStyle::PREVIEW:
				if (ctx.commit_text_preview != NULL)
				{
					std::string first = ctx.commit_text_preview;
					messages.push_back(std::string("ctx.preedit=") + first + '\n');
					messages.push_back(std::string("ctx.preedit.cursor=") +
						std::to_string(utf8towcslen(first.c_str(), 0)) + ',' +
						std::to_string(utf8towcslen(first.c_str(), first.size())) + '\n');
					break;
				}
				// no preview, fall back to composition
			case weasel::UIStyle::COMPOSITION:
				messages.push_back(std::string("ctx.preedit=") + ctx.composition.preedit + '\n');
				if (ctx.composition.sel_start <= ctx.composition.sel_end)
				{
					messages.push_back(std::string("ctx.preedit.cursor=") +
						std::to_string(utf8towcslen(ctx.composition.preedit, ctx.composition.sel_start)) + ',' +
						std::to_string(utf8towcslen(ctx.composition.preedit, ctx.composition.sel_end)) + '\n');
				}
				break;
			case weasel::UIStyle::PREVIEW_ALL:
				weasel::CandidateInfo cinfo;
				_GetCandidateInfo(cinfo, ctx);
				std::string topush = std::string("ctx.preedit=") + ctx.composition.preedit + "  [";
				for (auto i = 0; i < ctx.menu.num_candidates; i++)
				{
					std::string label = m_ui->style().label_font_point > 0 ? _GetLabelText(cinfo.labels, i, m_ui->style().label_text_format.c_str()) : "";
					std::string comment = m_ui->style().comment_font_point > 0 ? to_byte_string(cinfo.comments.at(i).str) : "";
					std::string mark_text = m_ui->style().mark_text.empty() ? "*" : to_byte_string(m_ui->style().mark_text);
					std::string prefix = (i != ctx.menu.highlighted_candidate_index) ? "" : mark_text;
					topush += " " + prefix + label + std::string(ctx.menu.candidates[i].text) + " " + comment;
				}
				messages.push_back(topush + " ]\n");
				//messages.push_back(std::string("ctx.preedit=") + ctx.composition.preedit + '\n');
				if (ctx.composition.sel_start <= ctx.composition.sel_end)
				{
					messages.push_back(std::string("ctx.preedit.cursor=") +
						std::to_string(utf8towcslen(ctx.composition.preedit, ctx.composition.sel_start)) + ',' +
						std::to_string(utf8towcslen(ctx.composition.preedit, ctx.composition.sel_end)) + '\n');
				}
				break;
			}
		}
		if (ctx.menu.num_candidates)
		{
			weasel::CandidateInfo cinfo;
			std::wstringstream ss;
			boost::archive::text_woarchive oa(ss);
			_GetCandidateInfo(cinfo, ctx);

			oa << cinfo;

			messages.push_back(std::string("ctx.cand=") + wcstoutf8(ss.str().c_str()) + '\n');
		}
		RimeFreeContext(&ctx);
	}

	// configuration information
	actions.insert("config");
	messages.push_back(std::string("config.inline_preedit=") + std::to_string((int)m_ui->style().inline_preedit) + '\n');

	// style
	bool has_synced = RimeGetOption(session_id, "__synced");
	if (!has_synced) {
		std::wstringstream ss;
		boost::archive::text_woarchive oa(ss);
		oa << m_ui->style();

		actions.insert("style");
		messages.push_back(std::string("style=") + wcstoutf8(ss.str().c_str()) + '\n');
		RimeSetOption(session_id, "__synced", true);
	}

	// summarize

	if (actions.empty())
	{
		messages.insert(messages.begin(), std::string("action=noop\n"));
	}
	else
	{
		std::string actionList(join(actions, ","));
		messages.insert(messages.begin(), std::string("action=") + actionList + '\n');
	}

	messages.push_back(std::string(".\n"));

	return std::all_of(messages.begin(), messages.end(), [&eat](std::string &msg)
	{
		return eat(std::wstring(utf8towcs(msg.c_str())));
	});
}

static inline COLORREF blend_colors(COLORREF fcolor, COLORREF bcolor)
{
	return RGB(
		(GetRValue(fcolor) * 2 + GetRValue(bcolor)) / 3,
		(GetGValue(fcolor) * 2 + GetGValue(bcolor)) / 3,
		(GetBValue(fcolor) * 2 + GetBValue(bcolor)) / 3
		) | ((((fcolor >> 24)+(bcolor >> 24)/2) << 24));
}

static Bool RimeConfigGetColor32b(RimeConfig* config, const char* key, int* value)
{
	int tmp = 0;
	if (!RimeConfigGetInt(config, key, &tmp)) return False;
	char color[16] = { 0 };
	if (!RimeConfigGetString(config, key, color, 16))
		return False;
	// hex number
	if (color[0] == '0' && (color[1]=='x' || color[1]=='X'))
	{
		if (strlen(color) <= 8 && strlen(color) > 2)	// 0xbbggrr ~ 0x? 
		{
			// 0xbbggrr
			RimeConfigGetInt(config, key, value);
			*value |= 0xff000000;
			return True;
		}
		else
			return RimeConfigGetInt(config, key, value);
	}
	// regular number or other stuff
	else
	{
		if (!RimeConfigGetInt(config, key, &tmp))
			return False;
		else
			*value = tmp | 0xff000000;
	}
	return True;
}

static void _UpdateUIStyle(RimeConfig* config, weasel::UI* ui, bool initialize)
{
	if (!ui) return;

	weasel::UIStyle &style(ui->style());

	const int BUF_SIZE = 2047;
	char buffer[BUF_SIZE + 1];
	memset(buffer, '\0', sizeof(buffer));
	if (RimeConfigGetString(config, "style/font_face", buffer, BUF_SIZE))
	{
		std::wstring tmp = utf8towcs(buffer);
		tmp = std::regex_replace(tmp, std::wregex(L"\\s*,\\s*"), L",");
		tmp = std::regex_replace(tmp, std::wregex(L"\\s*:\\s*"), L":");
		tmp = std::regex_replace(tmp, std::wregex(L"^\\s*|\\s*$"), L"");
		style.font_face = tmp;
	}
	memset(buffer, '\0', sizeof(buffer));
	if (RimeConfigGetString(config, "style/label_font_face", buffer, BUF_SIZE))
	{
		std::wstring tmp = utf8towcs(buffer);
		tmp = std::regex_replace(tmp, std::wregex(L"\\s*,\\s*"), L",");
		tmp = std::regex_replace(tmp, std::wregex(L"\\s*:\\s*"), L":");
		tmp = std::regex_replace(tmp, std::wregex(L"^\\s*|\\s*$"), L"");
		style.label_font_face = tmp;
	}
	memset(buffer, '\0', sizeof(buffer));
	if (RimeConfigGetString(config, "style/comment_font_face", buffer, BUF_SIZE))
	{
		std::wstring tmp = utf8towcs(buffer);
		tmp = std::regex_replace(tmp, std::wregex(L"\\s*,\\s*"), L",");
		tmp = std::regex_replace(tmp, std::wregex(L"\\s*:\\s*"), L":");
		tmp = std::regex_replace(tmp, std::wregex(L"^\\s*|\\s*$"), L"");
		style.comment_font_face = tmp;
	}
	RimeConfigGetInt(config, "style/font_point", &style.font_point);
	if (style.font_point <= 0)
		style.font_point = 12;
	if (!RimeConfigGetInt(config, "style/label_font_point", &style.label_font_point))
	{
		RimeConfigGetInt(config, "style/font_point", &style.label_font_point);
	}
	if (!RimeConfigGetInt(config, "style/comment_font_point", &style.comment_font_point))
	{
		RimeConfigGetInt(config, "style/font_point", &style.comment_font_point);
	}
	Bool inline_preedit = False;
	if (RimeConfigGetBool(config, "style/inline_preedit", &inline_preedit) || initialize)
	{
		style.inline_preedit = !!inline_preedit;
	}
	Bool color_font = True;
	if (RimeConfigGetBool(config, "style/color_font", &color_font) || initialize)
	{
		style.color_font = !!color_font;
	}

	// if system version lower than 8.1, disable color_font
	style.color_font = style.color_font && IsWindows8Point10OrGreaterEx();

	char preedit_type[20] = { 0 };
	if (RimeConfigGetString(config, "style/preedit_type", preedit_type, sizeof(preedit_type) - 1))
	{
		if (!std::strcmp(preedit_type, "composition"))
			style.preedit_type = weasel::UIStyle::COMPOSITION;
		else if (!std::strcmp(preedit_type, "preview"))
			style.preedit_type = weasel::UIStyle::PREVIEW;
		else if (!std::strcmp(preedit_type, "preview_all"))
			style.preedit_type = weasel::UIStyle::PREVIEW_ALL;
	}
	char capture_type[20] = { 0 };
	if (RimeConfigGetString(config, "style/capture_type", capture_type, sizeof(capture_type) - 1))
	{
		if (!std::strcmp(capture_type, "highlighted"))
			style.capture_type = weasel::UIStyle::CaptureType::HIGHLIGHTED;
		else if (!std::strcmp(capture_type, "candidates"))
			style.capture_type = weasel::UIStyle::CaptureType::CANDIDATES;
		else
			style.capture_type = weasel::UIStyle::CaptureType::NONE;
	}

	char align_type[20] = { 0 };
	if (RimeConfigGetString(config, "style/layout/align_type", align_type, sizeof(align_type) - 1))
	{
		if (!std::strcmp(align_type, "top"))
			style.align_type = weasel::UIStyle::ALIGN_TOP;
		else if (!std::strcmp(align_type, "center"))
			style.align_type = weasel::UIStyle::ALIGN_CENTER;
		else
			style.align_type = weasel::UIStyle::ALIGN_BOTTOM;
	}
	Bool display_tray_icon = False;
	if (RimeConfigGetBool(config, "style/display_tray_icon", &display_tray_icon) || initialize)
	{
		style.display_tray_icon = !!display_tray_icon;
	}
	Bool horizontal = False;
	if (RimeConfigGetBool(config, "style/horizontal", &horizontal) || initialize)
	{
		style.layout_type = horizontal ? weasel::UIStyle::LAYOUT_HORIZONTAL : weasel::UIStyle::LAYOUT_VERTICAL;
	}

	Bool fullscreen = False;
	if (RimeConfigGetBool(config, "style/fullscreen", &fullscreen) && fullscreen)
	{
		style.layout_type = (style.layout_type == weasel::UIStyle::LAYOUT_HORIZONTAL)
			 ? weasel::UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN : weasel::UIStyle::LAYOUT_VERTICAL_FULLSCREEN;
	}
	char label_text_format[128] = { 0 };
	if (RimeConfigGetString(config, "style/label_format", label_text_format, sizeof(label_text_format) - 1))
	{
		style.label_text_format = utf8towcs(label_text_format);
	}
	char mark_text[128] = { 0 };
	if (RimeConfigGetString(config, "style/mark_text", mark_text, sizeof(mark_text) - 1))
	{
		style.mark_text = utf8towcs(mark_text);
	}
	// layout (alternative to style/horizontal)
	char layout_type[256] = {0};
	if (RimeConfigGetString(config, "style/layout/type", layout_type, sizeof(layout_type) - 1))
	{
		if (!std::strcmp(layout_type, "vertical"))
			style.layout_type = weasel::UIStyle::LAYOUT_VERTICAL;
		else if (!std::strcmp(layout_type, "horizontal"))
			style.layout_type = weasel::UIStyle::LAYOUT_HORIZONTAL;
		if (!std::strcmp(layout_type, "vertical+fullscreen"))
			style.layout_type = weasel::UIStyle::LAYOUT_VERTICAL_FULLSCREEN;
		else if (!std::strcmp(layout_type, "horizontal+fullscreen"))
			style.layout_type = weasel::UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN;
		else
			LOG(WARNING) << "Invalid style type: " << layout_type;
	}
	RimeConfigGetInt(config, "style/layout/min_width", &style.min_width);
	RimeConfigGetInt(config, "style/layout/max_width", &style.max_width);
	RimeConfigGetInt(config, "style/layout/min_height", &style.min_height);
	if (!RimeConfigGetInt(config, "style/layout/border", &style.border)) {
		RimeConfigGetInt(config, "style/layout/border_width", &style.border);
	}
	RimeConfigGetInt(config, "style/layout/margin_x", &style.margin_x);
	RimeConfigGetInt(config, "style/layout/margin_y", &style.margin_y);
	RimeConfigGetInt(config, "style/layout/spacing", &style.spacing);
	RimeConfigGetInt(config, "style/layout/candidate_spacing", &style.candidate_spacing);
	RimeConfigGetInt(config, "style/layout/hilite_spacing", &style.hilite_spacing);
	RimeConfigGetInt(config, "style/layout/hilite_padding", &style.hilite_padding);
	style.hilite_padding = abs(style.hilite_padding);
	RimeConfigGetInt(config, "style/layout/shadow_radius", &style.shadow_radius);
	// negative shadow radius not allow
	if(style.shadow_radius < 0)
		style.shadow_radius = - style.shadow_radius;
	style.shadow_radius *= (!fullscreen);
	RimeConfigGetInt(config, "style/layout/shadow_offset_x", &style.shadow_offset_x);
	RimeConfigGetInt(config, "style/layout/shadow_offset_y", &style.shadow_offset_y);
	// round_corner as alias of hilited_corner_radius
	if(!RimeConfigGetInt(config, "style/layout/hilited_corner_radius", &style.round_corner))
	{
		RimeConfigGetInt(config, "style/layout/round_corner", &style.round_corner);
	}
	// neither round_corner_ex or corner_radius set, fallback to round_corner
	if(!RimeConfigGetInt(config, "style/layout/corner_radius", &style.round_corner_ex))
		RimeConfigGetInt(config, "style/layout/round_corner", &style.round_corner_ex);
	// fix padding and spacing settings
	if (style.hilite_padding * 2 > style.spacing)		// if hilite_padding over spacing, increase spacing
		style.spacing = style.hilite_padding * 2;
	if (style.hilite_padding * 2 > style.candidate_spacing)		// if hilite_padding over candidate spacing, increase candidate spacing
		style.candidate_spacing = style.hilite_padding * 2;
	if (style.hilite_padding > style.margin_x && style.margin_x >=0)		// if hilite_padiing over margin_x, increase margin_x
		style.margin_x = style.hilite_padding;
	else if (style.hilite_padding > -style.margin_x && style.margin_x < 0)
		style.margin_x = -(style.hilite_padding);
	if (style.hilite_padding > style.margin_y && style.margin_y >=0)		// if hilite_padiing over margin_y, increase margin_y
		style.margin_y = style.hilite_padding;
	else if (style.hilite_padding > -style.margin_y && style.margin_y < 0)
		style.margin_y = -(style.hilite_padding);
	// color scheme
	bool is_light = IsThemeLight();
	std::string color_pre = is_light ? "style/color_scheme" : "style/color_scheme_dark";
	//if (initialize && RimeConfigGetString(config, "style/color_scheme", buffer, BUF_SIZE))
	bool sta = RimeConfigGetString(config, color_pre.c_str(), buffer, BUF_SIZE);
	if (!sta)
		sta = RimeConfigGetString(config, "style/color_scheme", buffer, BUF_SIZE);
	if (initialize && sta)
	{
		std::string prefix("preset_color_schemes/");
		prefix += buffer;
		RimeConfigGetColor32b(config, (prefix + "/back_color").c_str(), &style.back_color);
		if (!RimeConfigGetColor32b(config, (prefix + "/shadow_color").c_str(), &style.shadow_color))
		{
			style.shadow_color = 0x00000000;
		}
		RimeConfigGetColor32b(config, (prefix + "/text_color").c_str(), &style.text_color);
		if (!RimeConfigGetColor32b(config, (prefix + "/candidate_text_color").c_str(), &style.candidate_text_color))
		{
			style.candidate_text_color = style.text_color;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/candidate_back_color").c_str(), &style.candidate_back_color))
		{
			style.candidate_back_color = style.back_color & 0x00ffffff;
		}

		if (!RimeConfigGetColor32b(config, (prefix + "/border_color").c_str(), &style.border_color))
		{
			style.border_color = style.text_color;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_text_color").c_str(), &style.hilited_text_color))
		{
			style.hilited_text_color = style.text_color;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_back_color").c_str(), &style.hilited_back_color))
		{
			style.hilited_back_color = style.back_color;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_candidate_text_color").c_str(), &style.hilited_candidate_text_color))
		{
			style.hilited_candidate_text_color = style.hilited_text_color;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_candidate_back_color").c_str(), &style.hilited_candidate_back_color))
		{
			style.hilited_candidate_back_color = style.hilited_back_color;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_candidate_shadow_color").c_str(), &style.hilited_candidate_shadow_color))
		{
			style.hilited_candidate_shadow_color = style.shadow_color  & 0x00ffffff;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_shadow_color").c_str(), &style.hilited_shadow_color))
		{
			style.hilited_shadow_color = style.shadow_color  & 0x00ffffff;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/candidate_shadow_color").c_str(), &style.candidate_shadow_color))
		{
			style.candidate_shadow_color = style.shadow_color & 0x00ffffff;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/label_color").c_str(), &style.label_text_color))
		{
			style.label_text_color = blend_colors(style.candidate_text_color, style.back_color);
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_label_color").c_str(), &style.hilited_label_text_color))
		{
			style.hilited_label_text_color = blend_colors(style.hilited_candidate_text_color, style.hilited_candidate_back_color);
		}
		style.comment_text_color = style.label_text_color;
		style.hilited_comment_text_color = style.hilited_label_text_color;
		if (RimeConfigGetColor32b(config, (prefix + "/comment_text_color").c_str(), &style.comment_text_color))
		{
			style.hilited_comment_text_color = style.comment_text_color;
		}
		RimeConfigGetColor32b(config, (prefix + "/hilited_comment_text_color").c_str(), &style.hilited_comment_text_color);
	}
}

static bool _UpdateUIStyleColor(RimeConfig* config, weasel::UIStyle& style, bool is_light)
{
	const int BUF_SIZE = 2047;
	char buffer[BUF_SIZE + 1];
	memset(buffer, '\0', sizeof(buffer));
	std::string color_mark = "style/color_scheme";
	if (!is_light)
		color_mark += "_dark";
	// color scheme
	if(RimeConfigGetString(config, color_mark.c_str(), buffer, BUF_SIZE))
	{
		std::string prefix("preset_color_schemes/");
		prefix += buffer;
		RimeConfigGetColor32b(config, (prefix + "/back_color").c_str(), &style.back_color);
		if (!RimeConfigGetColor32b(config, (prefix + "/shadow_color").c_str(), &style.shadow_color))
		{
			style.shadow_color = 0x00000000;
		}
		RimeConfigGetColor32b(config, (prefix + "/text_color").c_str(), &style.text_color);
		if (!RimeConfigGetColor32b(config, (prefix + "/candidate_text_color").c_str(), &style.candidate_text_color))
		{
			style.candidate_text_color = style.text_color;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/candidate_back_color").c_str(), &style.candidate_back_color))
		{
			style.candidate_back_color = style.back_color & 0x00ffffff;
		}

		if (!RimeConfigGetColor32b(config, (prefix + "/border_color").c_str(), &style.border_color))
		{
			style.border_color = style.text_color;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_text_color").c_str(), &style.hilited_text_color))
		{
			style.hilited_text_color = style.text_color;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_back_color").c_str(), &style.hilited_back_color))
		{
			style.hilited_back_color = style.back_color;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_candidate_text_color").c_str(), &style.hilited_candidate_text_color))
		{
			style.hilited_candidate_text_color = style.hilited_text_color;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_candidate_back_color").c_str(), &style.hilited_candidate_back_color))
		{
			style.hilited_candidate_back_color = style.hilited_back_color;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_candidate_shadow_color").c_str(), &style.hilited_candidate_shadow_color))
		{
			style.hilited_candidate_shadow_color = style.shadow_color  & 0x00ffffff;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_shadow_color").c_str(), &style.hilited_shadow_color))
		{
			style.hilited_shadow_color = style.shadow_color  & 0x00ffffff;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/candidate_shadow_color").c_str(), &style.candidate_shadow_color))
		{
			style.candidate_shadow_color = style.shadow_color  & 0x00ffffff;
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/label_color").c_str(), &style.label_text_color))
		{
			style.label_text_color = blend_colors(style.candidate_text_color, style.back_color);
		}
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_label_color").c_str(), &style.hilited_label_text_color))
		{
			style.hilited_label_text_color = blend_colors(style.hilited_candidate_text_color, style.hilited_candidate_back_color);
		}
		style.comment_text_color = style.label_text_color;
		style.hilited_comment_text_color = style.hilited_label_text_color;
		if (RimeConfigGetColor32b(config, (prefix + "/comment_text_color").c_str(), &style.comment_text_color))
		{
			style.hilited_comment_text_color = style.comment_text_color;
		}
		RimeConfigGetColor32b(config, (prefix + "/hilited_comment_text_color").c_str(), &style.hilited_comment_text_color);
		if (!RimeConfigGetColor32b(config, (prefix + "/hilited_mark_color").c_str(), &style.hilited_mark_color))
		{
			// default transparent hilited_candidate_back_color
			style.hilited_mark_color = style.hilited_candidate_back_color & 0x00ffffff;
		}
		return true;
	}
	return false;
}

static void CopyColorScheme(weasel::UIStyle& style, weasel::UIStyle src)
{
	style.text_color = src.text_color;
	style.candidate_text_color = src.candidate_text_color;
	style.candidate_back_color = src.candidate_back_color;
	style.candidate_shadow_color = src.candidate_shadow_color;
	style.label_text_color = src.label_text_color;
	style.comment_text_color = src.comment_text_color;
	style.back_color = src.back_color;
	style.shadow_color = src.shadow_color;
	style.border_color = src.border_color;
	style.hilited_text_color = src.hilited_text_color;
	style.hilited_back_color = src.hilited_back_color;
	style.hilited_shadow_color = src.hilited_shadow_color;
	style.hilited_candidate_text_color = src.hilited_candidate_text_color;
	style.hilited_candidate_back_color = src.hilited_candidate_back_color;
	style.hilited_candidate_shadow_color = src.hilited_candidate_shadow_color;
	style.hilited_label_text_color = src.hilited_label_text_color;
	style.hilited_comment_text_color = src.hilited_comment_text_color;
}

static void _LoadAppOptions(RimeConfig* config, AppOptionsByAppName& app_options)
{
	app_options.clear();
	RimeConfigIterator app_iter;
	RimeConfigIterator option_iter;
	RimeConfigBeginMap(&app_iter, config, "app_options");
	while (RimeConfigNext(&app_iter)) {
		AppOptions &options(app_options[app_iter.key]);
		RimeConfigBeginMap(&option_iter, config, app_iter.path);
		while (RimeConfigNext(&option_iter)) {
			Bool value = False;
			if (RimeConfigGetBool(config, option_iter.path, &value)) {
				options[option_iter.key] = !!value;
			}
		}
		RimeConfigEnd(&option_iter);
	}
	RimeConfigEnd(&app_iter);
}

void RimeWithWeaselHandler::_GetStatus(weasel::Status & stat, UINT session_id)
{
	RIME_STRUCT(RimeStatus, status);
	if (RimeGetStatus(session_id, &status))
	{
		std::string schema_id = status.schema_id;
		if (schema_id != m_last_schema_id)
		{
			m_last_schema_id = schema_id;
			RimeSetOption(session_id, "__synced", false); // Sync new schema options with front end
			_LoadSchemaSpecificSettings(schema_id);
		}
		stat.schema_name = utf8towcs(status.schema_name);
		stat.ascii_mode = !!status.is_ascii_mode;
		stat.composing = !!status.is_composing;
		stat.disabled = !!status.is_disabled;
		stat.full_shape = !!status.is_full_shape;
		RimeFreeStatus(&status);
	}

}

void RimeWithWeaselHandler::_GetContext(weasel::Context & weasel_context, UINT session_id)
{
	RIME_STRUCT(RimeContext, ctx);
	if (RimeGetContext(session_id, &ctx))
	{
		if (ctx.composition.length > 0)
		{
			weasel_context.preedit.str = utf8towcs(ctx.composition.preedit);
			if (ctx.composition.sel_start < ctx.composition.sel_end)
			{
				weasel::TextAttribute attr;
				attr.type = weasel::HIGHLIGHTED;
				attr.range.start = utf8towcslen(ctx.composition.preedit, ctx.composition.sel_start);
				attr.range.end = utf8towcslen(ctx.composition.preedit, ctx.composition.sel_end);

				weasel_context.preedit.attributes.push_back(attr);
			}
		}
		if (ctx.menu.num_candidates)
		{
			weasel::CandidateInfo &cinfo(weasel_context.cinfo);
			_GetCandidateInfo(cinfo, ctx);
		}
		RimeFreeContext(&ctx);
	}
}

bool RimeWithWeaselHandler::_IsSessionTSF(UINT session_id)
{
	static char client_type[20] = { 0 };
	RimeGetProperty(session_id, "client_type", client_type, sizeof(client_type) - 1);
	return std::string(client_type) == "tsf";
}
