#pragma once
#include <string>
#include <Windows.h>

namespace StringUtility {
// stringからwstringへの変換関数
std::wstring ConvertString(const std::string &str);

// wstringからstringへの変換関数
std::string ConvertString(const std::wstring &str);
} // namespace StringUtility
