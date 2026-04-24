#pragma once

#include <string>

namespace Unnamed {
	class AssetManager;
	class World;

	/// @brief Sequence固定tick回帰テストを実行するランナーです。
	class SequenceRegressionRunner final {
	public:
		/// @brief 回帰テストを実行します。
		/// @param world 評価対象ワールド
		/// @param assetManager アセット管理
		/// @param outReport テキストレポート出力先
		/// @return すべてのケースが成功した場合はtrue
		static bool RunAll(
			World&        world,
			AssetManager& assetManager,
			std::string*  outReport = nullptr
		);
	};
}

