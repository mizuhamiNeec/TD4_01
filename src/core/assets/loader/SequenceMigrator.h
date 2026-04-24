#pragma once

#include <cstdint>

#include <json.hpp>

namespace Unnamed {
	/// @brief Sequence JSONのバージョン移行を行うユーティリティです。
	class SequenceMigrator final {
	public:
		/// @brief JSONを最新バージョンへ移行します。
		/// @param ioRoot 入出力JSONルート
		/// @param outChanged 移行で変更があればtrue
		/// @param outSourceVersion 移行前バージョン
		/// @return JSONが有効で移行処理を実行できた場合はtrue
		static bool MigrateToLatest(
			nlohmann::json& ioRoot,
			bool*           outChanged       = nullptr,
			int32_t*        outSourceVersion = nullptr
		);
	};
}
