#pragma once
#include <memory>

#include "core/math/Math.h"
#include "core/math/Vec3.h"

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class GameMovementStateMachine;
	class TransformComponent;
	class BaseKinematicCollisionResolver;

	constexpr float kSimulationHz = 120.0f;

	struct MovementFrameInput {
		// 移動入力のベクトル
		Vec3 moveAxis = Vec3::zero;

		// 制御中のカメラの基底ベクトル
		Vec3 forward = Vec3::forward;
		Vec3 right   = Vec3::right;
		Vec3 up      = Vec3::up;

		// ジャンプ、しゃがみ、スプリントの入力状態
		bool jumpPressed   = false;
		bool crouchPressed = false;
		bool sprintPressed = false;

		bool noclip = false;
	};

	/// @brief 衝突付きでの移動を処理するコンポーネントの基底クラスです。
	class BaseCharacterComponent : public BaseComponent {
	public:
		using BaseComponent::BaseComponent;
		~BaseCharacterComponent() override;

		// ---- BaseCharacterComponent ----------------------------------------
		/// @brief 現在のフレームの入力を追加します。
		/// @param input 追加する入力情報
		/// @details プレイヤー、BOT、リプレイなど、様々な入力ソースから呼び出されることを想定しています。
		void AddMovementFrameInput(const MovementFrameInput& input);

		/// @brief 1ステップ分の移動シミュレーションを行います。
		/// @param transform
		/// @param input 現在のフレームの入力情報
		/// @param stepSeconds シミュレーションする時間（秒）
		virtual void SimulateStep(
			TransformComponent*       transform,
			const MovementFrameInput& input, float stepSeconds
		);

		// ---- BaseComponent ------------------------------------------------
		void OnAttached() override;

		void PrePhysicsTick(float deltaTime) override;
		void OnTick(float deltaTime) override;
		void PostPhysicsTick(float deltaTime) override;

		[[nodiscard]]
		std::string_view GetStableName() const override {
			return "game.BaseCharacterComponent";
		}

		[[nodiscard]]
		std::string_view GetComponentName() const override {
			return "BaseCharacterComponent";
		}

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	protected:
		[[nodiscard]] virtual TransformComponent* GetTransform() const;

		std::unique_ptr<GameMovementStateMachine> mStateMachine;

		std::unique_ptr<BaseKinematicCollisionResolver> mCollisionResolver;

		MovementFrameInput mMoveFrameInput;

		Vec3 mVelocity = Vec3::zero; // キャラクターの現在の速度

		bool mGrounded         = false;
		bool mCollisionEnabled = true;

		Vec3 mBoxHalfExtents = Vec3::zero;

		float    mAccumulator = 0.0f;                 // シミュレーションの時間積算値
		float    mSimStepSec  = 1.0f / kSimulationHz; // 120Hzでシミュレーション
		uint32_t mMaxSubSteps = 4;                    // 1フレームあたりの最大サブステップ数

		// プレイヤーによって制御されているかどうか
		bool mIsPlayerControlled = true;
	};
}
