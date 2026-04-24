#pragma once

#include <memory>
#include <string>

#include "engine/game/IGameWorldFactory.h"

namespace Unnamed {
	class World;
	class IDemoService;
	class ComponentRegistry;
	struct EngineServices;
	struct WorldServices;

	/// @brief ゲーム側から Engine へ機能注入するためのモジュール抽象です。
	class IGameModule : public IGameWorldFactory {
	public:
		~IGameModule() override = default;

		/// @brief Engine 初期化後にゲームモジュールを初期化します。
		virtual void Initialize(EngineServices& services) = 0;
		/// @brief Standalone 実行用のランタイムワールドを生成します。
		[[nodiscard]] virtual std::unique_ptr<World> CreateRuntimeWorld(
			const WorldServices& services
		) = 0;
		/// @brief Demo サービス実装を生成します。
		[[nodiscard]] virtual std::unique_ptr<IDemoService> CreateDemoService() = 0;
		/// @brief ゲーム固有コンポーネントを登録します。
		virtual void RegisterGameComponents(ComponentRegistry& componentRegistry) = 0;
		/// @brief 起動時のデフォルトシーンパスを返します。
		[[nodiscard]] virtual std::string GetDefaultStartupScenePath() const = 0;
		/// @brief UI ドキュメントのデフォルトパスを返します。
		/// @details Engine 側はこの値を利用し、ゲーム固有パスを直書きしません。
		[[nodiscard]] virtual std::string GetDefaultUiDocumentPath() const {
			return {};
		}
	};
}
