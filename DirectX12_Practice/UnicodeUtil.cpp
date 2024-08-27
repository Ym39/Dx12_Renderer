#include "UnicodeUtil.h"
#include <codecvt>
#include <string>
#include <vector>
#include <windows.h>

namespace UnicodeUtil
{
	bool GetPMXStringUTF16(std::ifstream& _file, std::wstring& output)
	{
		std::array<wchar_t, 512> wBuffer{};
		int textSize;

		_file.read(reinterpret_cast<char*>(&textSize), 4);

		_file.read(reinterpret_cast<char*>(&wBuffer), textSize);
		output = std::wstring(&wBuffer[0], &wBuffer[0] + textSize / 2);

		return true;
	}

	std::string WstringToString(const std::wstring& wstr)
	{
		if (wstr.empty()) return std::string();
		int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], -1, NULL, 0, NULL, NULL);
		std::string strTo(sizeNeeded - 1, 0); 
		WideCharToMultiByte(CP_UTF8, 0, &wstr[0], -1, &strTo[0], sizeNeeded, NULL, NULL);
		return strTo;
	}

	std::string convertShiftJisToUtf8(const std::string& shiftJisText) {
		// Create a wide string from the shift_jis text
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
		std::wstring wideText = converter.from_bytes(shiftJisText);

		// Convert the wide string to utf-8 encoded string
		std::string utf8Text = converter.to_bytes(wideText);

		return utf8Text;
	}

	_locale_t jpn = ::_create_locale(LC_ALL, "jpn");

	std::wstring multi_to_wide_capi(std::string const& src)
	{
		try
		{
			std::size_t converted{};
			std::vector<wchar_t> dest(src.size(), L'\0');
			if (::_mbstowcs_s_l(&converted, dest.data(), dest.size(), src.data(), _TRUNCATE, jpn) != 0) {
				throw std::system_error{ errno, std::system_category() };
			}
			dest.resize(std::char_traits<wchar_t>::length(dest.data()));
			dest.shrink_to_fit();
			return std::wstring(dest.begin(), dest.end());
		}
		catch (std::system_error error)
		{
			return std::wstring(src.begin(), src.end());
		}
	}

	std::wstring multi_to_wide_capi_set_buffer(wchar_t buffer[128], std::string const& src)
	{
		try
		{
			std::size_t converted{};
			if (::_mbstowcs_s_l(&converted, buffer, src.size(), src.data(), _TRUNCATE, jpn) != 0) {
				throw std::system_error{ errno, std::system_category() };
			}
			buffer[src.size() + 1] = L'\0';
			std::size_t length = std::char_traits<wchar_t>::length(buffer);
			return std::wstring(buffer, buffer + length);
		}
		catch (std::system_error error)
		{
			return std::wstring(src.begin(), src.end());
		}
	}

	std::string wide_to_utf8_cppapi(std::wstring const& src)
	{
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.to_bytes(src);
	}

	std::string multi_to_utf8_cppapi(std::string const& src)
	{
		auto const wide = multi_to_wide_capi(src);
		return wide_to_utf8_cppapi(wide);
	}

	void ReadJISToWString(std::ifstream& _file, std::wstring& output, size_t length)
	{
		char* charStr = new char[length];
		_file.read(reinterpret_cast<char*>(charStr), length);
		std::string jisString = charStr;
		output = multi_to_wide_capi(jisString);
		delete[] charStr;
	}

	bool GetPMXStringUTF8(std::ifstream& _file, std::string& output)
	{
		std::array<wchar_t, 512> wBuffer{};
		int textSize;

		_file.read(reinterpret_cast<char*>(&textSize), 4);
		_file.read(reinterpret_cast<char*>(&wBuffer), textSize);

		output = std::string(&wBuffer[0], &wBuffer[0] + textSize);

		return true;
	}
}
