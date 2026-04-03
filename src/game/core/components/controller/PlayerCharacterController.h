#pragma once

#include <json.hpp>
#include "core/math/Vec2.h"

#include "../character/base/BaseCharacterComponent.h"

#include "base/BaseCharacterController.h"
#include "game/core/replay/DemoTypes.h"

namespace Unnamed {
	class CameraRotatorComponent;

	/// @brief プレイヤーがキャラクターを制御するためのコンポーネントです。
	class PlayerCharacterController : public BaseCharacterController {
	public:
		~PlayerCharacterController() override;

		// ---- BaseComponent ------------------------------------------------
		void OnAttached() override;
		void OnDetached() override;

		void                           PrePhysicsTick(float deltaTime) override;
		void                           OnFrameInputTick(float frameDeltaTime) override;
		[[nodiscard]] TICK_GROUP       GetTickGroup() const override {
			return TICK_GROUP::EARLY;
		}
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void WriteReplayState(nlohmann::json& outState) const;
		void ReadReplayState(const nlohmann::json& inState);
		[[nodiscard]] uint64_t ComputeReplayStateHash() const;

	protected:
		void TryBindCameraRotator();
		[[nodiscard]] MovementFrameInput BuildMovementFrameInput();
		[[nodiscard]] DemoTickCommand    BuildPlayerTickCommand(uint64_t tick);
		void ApplyLookFromCommand(const DemoTickCommand& command, float stepSeconds);

		InputSystem*             mInput = nullptr;
		CameraRotatorComponent*  mCameraRotator = nullptr;
		MovementFrameInput       mDebugMoveFrameInput = {};
		uint64_t                 mFixedTickCounter = 0;
		float                    mLastViewYawDeg = 0.0f;
		float                    mLastViewPitchDeg = 0.0f;
		uint32_t                 mQueuedSprintPressCount = 0;
		bool                     mRecordingInitialSnapshotCaptured = false;
		bool                     mWasRecordingMode = false;
		bool                     mWasPlaybackMode = false;
	};
}
