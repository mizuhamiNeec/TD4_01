#include <fstream>

#include <core/ini/IniParser.h>
#include <engine/OldConsole/Console.h>

#include "core/string/StrUtil.h"

/// @brief INIファイルをパースする
/// @param filePath INIファイルのパス
/// @return セクション名とキー・値のペアのマップ
std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
IniParser::ParseIniFile(
	const std::string& filePath
) {
	std::unordered_map<std::string, std::unordered_map<
		                   std::string, std::string>> iniData;
	std::ifstream                                     inputFile(filePath);
	if (!inputFile.is_open()) {
		Console::Print("ファイルを開けませんでした: " + filePath + "\n", kConTextColorError);
		return iniData;
	}

	std::string line;
	std::string currentSection = "Default";
	while (std::getline(inputFile, line)) {
		// コメント、空行は無視
		line = StrUtil::TrimSpaces(line);
		if (line.empty() || line[0] == ';') {
			continue;
		}

		// セクションの判定
		if (line.front() == '[' && line.back() == ']') {
			currentSection = line.substr(1, line.size() - 2);
			continue;
		}

		// キーと値の分割
		size_t equalsPos = line.find('=');
		if (equalsPos != std::string::npos) {
			std::string key = StrUtil::TrimSpaces(line.substr(0, equalsPos));
			std::string value = StrUtil::TrimSpaces(line.substr(equalsPos + 1));
			iniData[currentSection][key] = value;
		}
	}

	return iniData;
}
