#pragma once
#include <stdexcept>
#include <stdint.h>
#include <array>
#include <fstream>

namespace UnicodeUtil
{
	bool GetPMXStringUTF16(std::ifstream& _file, std::wstring& output);
	void ReadJISToWString(std::ifstream& _file, std::wstring& output, size_t length);
	bool GetPMXStringUTF8(std::ifstream& _file, std::string& output);
	std::string WstringToString(const std::wstring& wstr);
}
