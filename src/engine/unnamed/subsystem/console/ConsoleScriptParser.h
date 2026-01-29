#pragma once
#include <string_view>

namespace Unnamed {
	/// @brief コンソールスクリプトパーサークラス
	/// @details コンソールスクリプト(.cfg)を解析してコマンドを実行するクラス
	class ConsoleScriptParser {
	public:
		ConsoleScriptParser(const std::string_view& path);
	};
}
