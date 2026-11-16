#pragma once
#include <ostream>

namespace Logger {

// デバッグ用のログ出力関数
void Log(std::ostream &os, const std::string &message);

} // namespace Logger
