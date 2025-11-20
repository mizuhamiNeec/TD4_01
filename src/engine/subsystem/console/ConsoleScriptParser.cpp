#include "ConsoleScriptParser.h"

#include <fstream>

#include <engine/subsystem/console/Log.h>

#include "core/string/StrUtil.h"

#include "engine/OldConsole/Console.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "ConsoleScriptParser";

	/// @brief コンストラクタ
	/// @param path スクリプトファイルのパス
	ConsoleScriptParser::ConsoleScriptParser(const std::string_view& path) {
		std::ifstream inputFile(path.data());

		if (!inputFile.is_open()) {
			Error(
				kChannel, "Failed to open script file: {}", std::string(path)
			);
			throw std::runtime_error("Failed to open script file");
		}

		Msg(kChannel, "Executing script file: {}", std::string(path));

		std::string line;
		while (std::getline(inputFile, line)) {
			line = StrUtil::TrimSpaces(line);
			// 空行またはコメント行をスキップ
			if (
				line.empty() ||
				line[0] == ';' ||
				line[0] == '/' && line.size() > 1 && line[1] == '/'
			) {
				continue;
			}

			// TODO: とりあえず新旧両方で実行
			Console::SubmitCommand(line);
			ServiceLocator::Get<ConsoleSystem>()->ExecuteCommand(line);
		}
	}
}
