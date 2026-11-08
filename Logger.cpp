#include "Logger.h"
#include <string>
#include <Windows.h>

namespace Logger {
void Log(std::ostream &os, const std::string &message) {
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}
}// デバッグ用のログ出力関数
