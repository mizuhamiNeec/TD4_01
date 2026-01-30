#pragma once

#include <engine/state/IEngineModeState.h>

namespace Unnamed {
	/// @brief エディターモードの State
	/// @details ImGui/Editor の更新・描画、およびビューポート情報の更新を担当する。
	class EditorModeState final : public IEngineModeState {
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
