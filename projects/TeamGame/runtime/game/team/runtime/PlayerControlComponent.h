#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <core/math/Vec2.h>
#include <string>

namespace MyGame {

	class PlayerMoveComponent;

	/// @brief プレイヤーの入力を受け付けるコントローラーコンポーネント
	/// InputSystemから入力を取得して、PlayerMoveComponentに指令を送る
	class PlayerControlComponent : public Unnamed::BaseComponent {
	public:
		// -----------------------------------------------------------------------
		// ライフサイクル
		// -----------------------------------------------------------------------

		/// コンポーネントがアタッチされたときに呼び出される
		void OnAttached() override;

		/// 毎フレーム更新 - 入力を処理
		void OnTick(float deltaTime) override;

		/// コンポーネントがデタッチされたときに呼び出される
		void OnDetached() override;

		// -----------------------------------------------------------------------
		// 入力設定
		// -----------------------------------------------------------------------

		/// @brief 移動軸の名前を設定（デフォルト: "MoveAxis"）
		void SetMoveAxisName(const std::string& axisName);

		/// @brief ジャンプアクションの名前を設定（デフォルト: "Jump"）
		void SetJumpActionName(const std::string& actionName);

		/// @brief 現在の移動入力を取得
		[[nodiscard]] Vec2 GetMoveInput() const;

		// -----------------------------------------------------------------------
		// BaseComponent override
		// -----------------------------------------------------------------------

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		/// コンポーネントの値を読み込む際に使用されます
		void Deserialize(const Unnamed::JsonReader& reader) override;

		/// コンポーネントの値を書き込む際に使用されます
		void Serialize(Unnamed::JsonWriter& writer) const override;

	private:
		// -----------------------------------------------------------------------
		// 入力設定
		// -----------------------------------------------------------------------

		/// 移動軸の名前
		std::string _moveAxisName = "MoveAxis";

		/// ジャンプアクションの名前
		std::string _jumpActionName = "Jump";

		/// 現在の移動入力（キャッシュ）
		Vec2 _currentMoveInput = Vec2::zero;

		// -----------------------------------------------------------------------
		// 内部処理
		// -----------------------------------------------------------------------

		/// PlayerMoveComponent へのポインタをキャッシュ
		PlayerMoveComponent* _playerMoveComponent = nullptr;

		/// @brief PlayerMoveComponent を取得・キャッシュする
		[[nodiscard]] PlayerMoveComponent* GetOrCachePlayerMoveComponent();

		/// @brief InputSystem からキーバインディングを設定
		/// WASD で移動、Space でジャンプ
		void SetupInputBindings();

		/// @brief InputSystem から入力を取得し、PlayerMoveComponent に適用
		void ProcessInput();
	};

}
