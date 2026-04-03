#pragma once
#include <cstdint>
#include <string>
#include <string_view>

#include <json.hpp>

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
		
		void OnEditorTick(float deltaTime) override;

		[[nodiscard]] TICK_GROUP GetTickGroup() const override {
			return TICK_GROUP::GAMEPLAY;
		}

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		virtual void                   WriteReplayState(nlohmann::json& outState) const;
		virtual void                   ReadReplayState(const nlohmann::json& inState);
		[[nodiscard]] virtual uint64_t ComputeReplayStateHash() const;

	protected:
		virtual void RegisterMovementStates(
			GameMovementStateMachine& stateMachine
		);
		[[nodiscard]] virtual std::string GetInitialStateName() const;
		virtual void UpdateCollisionHull(TransformComponent* transform) const;

		[[nodiscard]] TransformComponent* GetTransform() const override;
		[[nodiscard]] Vec3                ResolveSupportLinearVelocity(
			uint64_t supportEntityGuid
		) const;
		[[nodiscard]] Vec3 ResolveSupportStepDelta(
			uint64_t supportEntityGuid, float stepSeconds
		) const;
		[[nodiscard]] bool ApplyPassiveMotionStep(
			TransformComponent* transform, float stepSeconds
		);
		void PublishCue(
			std::string id, float value = 0.0f, float value2 = 0.0f
		);
		static std::string ToLowerCopy(std::string_view text);
		virtual void OnAfterCoreCueDispatch(
			std::string_view          previousStateName,
			std::string_view          currentStateName,
			const MovementFrameInput& input,
			bool                      wasGrounded,
			bool                      isGrounded,
			float                     stepSeconds
		);

		Physics::Engine* mPhysics = nullptr;
		InputSystem*     mInput   = nullptr;
		ConsoleSystem*   mConsole = nullptr;

		struct SupportCache {
			bool     grounded              = false;
			uint64_t supportEntityGuid     = 0;
			Vec3     supportLinearVelocity = Vec3::zero;
			Vec3     supportStepDelta      = Vec3::zero;
		};

		SupportCache mSupportCache;
		float        mJumpSnapDisableRemaining = 0.0f;

#ifdef _DEBUG
		std::string mDebugLastPublishedCueId;
		float       mDebugLastPublishedCueValue  = 0.0f;
		float       mDebugLastPublishedCueValue2 = 0.0f;
		uint64_t    mDebugPublishedCueCount      = 0;
#endif
	};
}
