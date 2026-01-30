#pragma once

#include <engine/state/IEngineModeState.h>

namespace Unnamed {
	/// @brief ゲームモードの State
	/// @details SceneManager の更新・描画とビューポートをウィンドウ全体に設定
	class GameModeState final : public IEngineModeState {
	public:
		/// @brief State へ入る
		void OnEnter(Engine& engine) override;

		/// @brief State から出る
		void OnExit(Engine& engine) override;

		/// @brief 更新
		void Update(Engine& engine, float deltaTime) override;

		/// @brief 描画
		void Render(Engine& engine) override;
	};
}
