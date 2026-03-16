#pragma once
#include <filesystem>
#include <string>

namespace Path {
	/// @brief ベースディレクトリからの相対パスを解決する。パスがすでに絶対パスの場合は、そのまま正規化して返す。
	/// @param baseDir ベースディレクトリ
	/// @param path 解決するパス
	/// @return 解決されたパス
	std::string ResolveRelativePath(
		const std::filesystem::path& baseDir, std::string path
	);
}
