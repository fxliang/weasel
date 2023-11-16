#pragma once
#include <string>

inline int utf8towcslen(const char* utf8_str, int utf8_len)
{
	return MultiByteToWideChar(CP_UTF8, 0, utf8_str, utf8_len, NULL, 0);
}

inline std::wstring getUsername() {
	DWORD len = 0;
	GetUserName(NULL, &len);

	if (len <= 0) {
		return L"";
	}

	wchar_t *username = new wchar_t[len + 1];

	GetUserName(username, &len);
	if (len <= 0) {
		delete[] username;
		return L"";
	}
	auto res = std::wstring(username);
	delete[] username;
	return res;
}

// data directories
std::wstring WeaselUserDataPath();

const char* weasel_shared_data_dir();
const char* weasel_shared_data_dir_x64();
const char* weasel_user_data_dir();

inline std::wstring string_to_wstring(const std::string& str, int code_page = CP_ACP)
{
	// support CP_ACP and CP_UTF8 only
	if (code_page != 0 && code_page != CP_UTF8)	return L"";
	// calc len
	int len = MultiByteToWideChar(code_page, 0, str.c_str(), (int)str.size(), NULL, 0);
	if(len <= 0)    return L"";
	std::wstring res;
	TCHAR* buffer = new TCHAR[len + 1];
	MultiByteToWideChar(code_page, 0, str.c_str(), (int)str.size(), buffer, len);
	buffer[len] = '\0';
	res.append(buffer);
	delete[] buffer;
	return res;
}

inline std::string wstring_to_string(const std::wstring& wstr, int code_page = CP_ACP)
{
	// support CP_ACP and CP_UTF8 only
	if (code_page != 0 && code_page != CP_UTF8)	return "";
	int len = WideCharToMultiByte(code_page, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
	if(len <= 0)    return "";
	std::string res;
	char* buffer = new char[len + 1];
	WideCharToMultiByte(code_page, 0, wstr.c_str(), (int)wstr.size(), buffer, len, NULL, NULL);
	buffer[len] = '\0';
	res.append(buffer);
	delete[] buffer;
	return res;
}

inline bool IfFileExistW(std::wstring filepathw)
{
	DWORD dwAttrib = GetFileAttributes(filepathw.c_str());
	return (INVALID_FILE_ATTRIBUTES != dwAttrib && 0 == (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

inline bool IfFileExist(std::string filepath, int code_page = CP_ACP)
{
	std::wstring filepathw{string_to_wstring(filepath, code_page)};
	DWORD dwAttrib = GetFileAttributes(filepathw.c_str());
	return (INVALID_FILE_ATTRIBUTES != dwAttrib && 0 == (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

inline bool is_wow64() {
	DWORD errorCode;
	if (GetSystemWow64DirectoryW(NULL, 0) == 0)
		if ((errorCode = GetLastError()) == ERROR_CALL_NOT_IMPLEMENTED)
			return false;
		else
			ExitProcess((UINT)errorCode);
	else
		return true;
}

// resource
std::string GetCustomResource(const char *name, const char *type);
