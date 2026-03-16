#pragma once
#include <string>

#include "base/BaseCharacterComponent.h"

namespace Unnamed::Physics {
	class Engine;
}

namespace Unnamed {
	class TransformComponent;
	class InputSystem;
	class ConsoleSystem;
	template <typename T>
	class ConVar;

	/// @brief プレイヤーの移動を処理するコンポーネントです。
	class GameMovementComponent : public BaseCharacterComponent {
	public:
		~GameMovementComponent() override;

		// ---- GameMovementComponent -----------------------------------------
		void SimulateStep(
			TransformComponent* transform, const MovementFrameInput& input,
			float               stepSeconds
		) override;

		// ---- BaseComponent ------------------------------------------------
		void OnAttached() override;

		void PrePhysicsTick(float deltaTime) override;
		void OnTick(float deltaTime) override;
		void PostPhysicsTick(float deltaTime) override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	protected:
		virtual void RegisterMovementStates(
			GameMovementStateMachine& stateMachine
		);
		[[nodiscard]] virtual std::string GetInitialStateName() const;
		virtual void UpdateCollisionHull(TransformComponent* transform) const;

		[[nodiscard]] TransformComponent* GetTransform() const override;

		Physics::Engine* mPhysics = nullptr;
		InputSystem*     mInput   = nullptr;
		ConsoleSystem*   mConsole = nullptr;

		// Walk
		float mCurrentMaxWalkSpeed = 0.0f; // 現在の最大歩行速度

		// ---- ConVars -------------------------------------------------------
		// Cheats
		ConVar<bool>* mNoclip = nullptr;

		// SV
		ConVar<float>* mAccelerate    = nullptr;
		ConVar<float>* mAirAccelerate = nullptr;
		ConVar<float>* mMaxSpeed      = nullptr;
		ConVar<float>* mStopSpeed     = nullptr;
		ConVar<float>* mAirSpeedCap   = nullptr;
		ConVar<float>* mFriction      = nullptr;

		// Ground
		ConVar<float>* mDuckSpeed   = nullptr;
		ConVar<float>* mWalkSpeed   = nullptr;
		ConVar<float>* mSprintSpeed = nullptr;

		// Jump
		ConVar<float>* mJumpVelocity = nullptr;
	};
}
