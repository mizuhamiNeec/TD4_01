#pragma once

#include <cstdint>

namespace Unnamed {
	class Engine;

	/// @brief Engine の動作モードを表すステートインターフェース
	/// @details EngineはIEngineModeStateに Update/Render を任せる
	class IEngineModeState {
	public:
		/// @brief デストラクタ
		virtual ~IEngineModeState() = default;

		/// @brief 状態遷移でこの State に入る際に呼ばれます
		virtual void OnEnter(Engine& engine) = 0;

		/// @brief 状態遷移でこの State から出る際に呼ばれます
		virtual void OnExit(Engine& engine) = 0;

		/// @brief 更新処理
		virtual void Update(Engine& engine, float deltaTime) = 0;

		/// @brief 描画処理
		virtual void Render(Engine& engine) = 0;
	};
}
