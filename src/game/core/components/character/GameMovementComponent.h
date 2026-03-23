#pragma once
#include <cstdint>
#include <string>

#include "base/BaseCharacterComponent.h"

namespace Unnamed::Physics {
	class Engine;
}

namespace Unnamed {
	class TransformComponent;
	class InputSystem;
	class ConsoleSystem;
	class KinematicMoverComponent;
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
		[[nodiscard]] TickGroup GetTickGroup() const override {
			return TickGroup::Gameplay;
		}

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
		[[nodiscard]] Vec3 ResolveSupportLinearVelocity(
			uint64_t supportEntityGuid
		) const;
		[[nodiscard]] Vec3 ResolveSupportStepDelta(
			uint64_t supportEntityGuid, float stepSeconds
		) const;
		[[nodiscard]] bool ApplyPassiveMotionStep(
			TransformComponent* transform, float stepSeconds
		);

		Physics::Engine* mPhysics = nullptr;
		InputSystem*     mInput   = nullptr;
		ConsoleSystem*   mConsole = nullptr;

		struct SupportCache {
			bool     grounded            = false;
			uint64_t supportEntityGuid   = 0;
			Vec3     supportLinearVelocity = Vec3::zero;
			Vec3     supportStepDelta      = Vec3::zero;
		};

		SupportCache mSupportCache;
		float        mJumpSnapDisableRemaining = 0.0f;
		ConVar<float>* mPassivePushContactSkin = nullptr;
		ConVar<int>*   mPassivePushMaxDepenetrationIters = nullptr;
	};
}
