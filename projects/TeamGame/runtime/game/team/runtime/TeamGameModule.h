#pragma once

#include "engine/game/IGameModule.h"

// 前方宣言
class MagVoiceBridge;

namespace Unnamed {
	/// @brief TeamGame 向けの最小 GameModule 実装です。
	class TeamGameModule final : public IGameModule {
	public:
		/// @brief モジュールを初期化します。
		void Initialize(EngineServices& services) override;
		/// @brief Standalone 向けランタイムワールドを生成します。
		[[nodiscard]] std::unique_ptr<World> CreateRuntimeWorld(
			const WorldServices& services
		) override;
		/// @brief PIE 向けワールドを生成します。
		[[nodiscard]] std::unique_ptr<World> CreatePlayWorld(
			const WorldServices& services
		) override;
		/// @brief Demo サービス実装を生成します。
		[[nodiscard]] std::unique_ptr<IDemoService> CreateDemoService() override;
		/// @brief ゲーム固有コンポーネントを登録します。
		void RegisterGameComponents(ComponentRegistry& componentRegistry) override;
		/// @brief ゲーム名・ルート・既定シーン情報を返します。
		[[nodiscard]] GameModulePaths GetGameModulePaths() const override;
		/// @brief 起動時デフォルトシーンパスを返します。
		[[nodiscard]] std::string GetDefaultStartupScenePath() const override;
		/// @brief UI ドキュメントのデフォルトパスを返します。
		[[nodiscard]] std::string GetDefaultUiDocumentPath() const override;

	private:
		/// @brief ゲーム開始時に MagVoiceBridge を初期化・起動
		void InitializeMagVoiceBridge();

		/// @brief グローバルな MagVoiceBridge インスタンスを設定
		void SetGlobalMagVoiceBridge(MagVoiceBridge* bridge);
	};

	/// @brief TeamGame GameModule を生成します。
	[[nodiscard]] std::unique_ptr<IGameModule> CreateTeamGameModule();
}
