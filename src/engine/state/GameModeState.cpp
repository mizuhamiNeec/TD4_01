#include <pch.h>

#include <engine/state/GameModeState.h>

#include <engine/Engine.h>

namespace Unnamed {
	void GameModeState::OnEnter(Engine& engine) {
		// ゲームモードではエディターは不要
		engine.DestroyEditor();
	}

	void GameModeState::OnExit(Engine& engine) {
		// なんもしない
		(void)engine;
	}

	void GameModeState::Update(Engine& engine, float deltaTime) {
		// Scene 更新
		if (auto* sm = engine.GetSceneManagerInstance()) {
			sm->Update(deltaTime);
		}

		// ビューポートをウィンドウ全体へ設定
		engine.SetViewportToMainWindow();
	}

	void GameModeState::Render(Engine& engine) {
		// Scene 描画
		if (auto* sm = engine.GetSceneManagerInstance()) { sm->Render(); }
	}
}
