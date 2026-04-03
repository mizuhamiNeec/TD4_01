#include "PlayerCharacterController.h"

#include "../CameraRotatorComponent.h"
#include "../character/base/BaseCharacterComponent.h"

#include "core/ComponentRegistry.h"

#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/unnamed/subsystem/input/device/gamepad/GamepadDevice.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/World.h"

#include "game/core/replay/DemoManager.h"
#include "game/core/replay/ReplayHash.h"

namespace Unnamed {
	namespace {
		void BindGamepadMoveAxisOnce(InputSystem* input) {
			static bool sBound = false;
			if (sBound || !input) {
				return;
			}

			constexpr InputKey gamepadMoveX = {
				.device = InputDeviceType::GAMEPAD,
				.code   = VG_LX
			};
			constexpr InputKey gamepadMoveY = {
				.device = InputDeviceType::GAMEPAD,
				.code   = VG_LY
			};

			input->BindAxis2D("GamepadMove", gamepadMoveX, INPUT_AXIS::X, 1.0f);
			input->BindAxis2D("GamepadMove", gamepadMoveY, INPUT_AXIS::Y, 1.0f);
			sBound = true;
		}
	}

	PlayerCharacterController::~PlayerCharacterController() = default;

	void PlayerCharacterController::OnAttached() {
		BaseCharacterController::OnAttached();

		if (auto* inputSystem = ServiceLocator::Get<InputSystem>()) {
			mInput = inputSystem;
			BindGamepadMoveAxisOnce(mInput);
		} else {
			Error(
				GetComponentName(),
				"UInputSystemを取得できませんでした。BaseCharacterControllerは入力を処理できません。"
			);
		}

		TryBindCameraRotator();
		if (mCameraRotator) {
			const Vec2 lookDegrees = mCameraRotator->GetLookAnglesDegrees();
			mLastViewPitchDeg      = lookDegrees.x;
			mLastViewYawDeg        = lookDegrees.y;
		}

		mFixedTickCounter     = 0;
		mQueuedSprintPressCount = 0;
		mRecordingInitialSnapshotCaptured = false;
		mWasRecordingMode = false;
		mWasPlaybackMode       = false;
	}

	void PlayerCharacterController::OnDetached() {
		if (mCameraRotator) {
			mCameraRotator->SetDirectInputEnabled(true);
			mCameraRotator = nullptr;
		}
		mInput = nullptr;
		mQueuedSprintPressCount = 0;
		mRecordingInitialSnapshotCaptured = false;
		mWasRecordingMode = false;
		mWasPlaybackMode       = false;

		BaseCharacterController::OnDetached();
	}

	void PlayerCharacterController::PrePhysicsTick(const float deltaTime) {
		(void)deltaTime;
		if (!mTarget) {
			return;
		}

		DemoManager* demoManager = ServiceLocator::Get<DemoManager>();
		const float fixedTickSeconds =
			demoManager ? demoManager->GetSimulationStepSeconds() :
			              DemoManager::TickStepSecondsFromRate(
				              DemoManager::ResolveConfiguredTickRate()
			              );

		const uint64_t subjectEntityGuid = GetOwner() ? GetOwner()->GetGuid() : 0;
		bool           playbackMode      = demoManager && demoManager->IsPlayback();
		bool           recordingMode     = demoManager && demoManager->IsRecording();

		if (recordingMode && !mWasRecordingMode) {
			mRecordingInitialSnapshotCaptured = false;
		} else if (!recordingMode) {
			mRecordingInitialSnapshotCaptured = false;
		}

		if (playbackMode && !mWasPlaybackMode) {
			mFixedTickCounter     = demoManager->GetPlaybackStartTick();
			mQueuedSprintPressCount = 0;
			mTarget->ClearDeterministicInputQueue();
			if (Entity* owner = GetOwner()) {
				if (!demoManager->ApplyInitialSnapshotIfNeeded(*owner)) {
					(void)demoManager->Stop();
					playbackMode     = false;
					mWasPlaybackMode = false;
					return;
				}
			}
		}
		mWasPlaybackMode = playbackMode;
		mWasRecordingMode = recordingMode;

		DemoTickCommand command = {};
		bool            usedPlaybackCommand = false;

		if (demoManager && playbackMode) {
			usedPlaybackCommand = demoManager->ConsumePlaybackCommand(
				mFixedTickCounter,
				subjectEntityGuid,
				command
			);
			if (usedPlaybackCommand) {
				ApplyLookFromCommand(command, fixedTickSeconds);
			} else {
				const bool finished = demoManager->IsPlaybackFinished();
				if (finished) {
					Warning(
						GetComponentName(),
						"Demo playback reached EOF at tick {}. Returning to live input.",
						mFixedTickCounter
					);
				} else {
					Warning(
						GetComponentName(),
						"Demo playback has no command for tick {}. Stopping playback.",
						mFixedTickCounter
					);
				}
				(void)demoManager->Stop();
				playbackMode      = false;
				mWasPlaybackMode  = false;
				mQueuedSprintPressCount = 0;
			}
		}

		if (!usedPlaybackCommand) {
			if (demoManager && recordingMode &&
			    !mRecordingInitialSnapshotCaptured) {
				if (Entity* owner = GetOwner()) {
					demoManager->CaptureInitialSnapshotIfNeeded(*owner);
					mRecordingInitialSnapshotCaptured = true;
				}
			}

			if (!mCameraRotator) {
				TryBindCameraRotator();
			}

			if (mCameraRotator) {
				const Vec2 lookDegrees = mCameraRotator->GetLookAnglesDegrees();
				mLastViewPitchDeg      = lookDegrees.x;
				mLastViewYawDeg        = lookDegrees.y;
			}

			command = BuildPlayerTickCommand(mFixedTickCounter);
			if (demoManager && demoManager->IsRecording()) {
				demoManager->SubmitLiveCommand(command);
			}
		}

		mDebugMoveFrameInput = command.playerInput.movement;
		mTarget->EnqueueDeterministicInput(
			command.tick,
			fixedTickSeconds,
			command.playerInput.movement
		);

		++mFixedTickCounter;
	}

	void PlayerCharacterController::OnFrameInputTick(const float frameDeltaTime) {
		if (!mTarget || !mInput) {
			return;
		}

		if (auto* demoManager = ServiceLocator::Get<DemoManager>();
			demoManager && demoManager->IsPlayback()) {
			return;
		}

		// 入力は描画フレームごとに回収して、低tickrate時の取りこぼしを防ぐ。
		if (mInput->IsPressed("sprint")) {
			mQueuedSprintPressCount = std::min(
				mQueuedSprintPressCount + 1u,
				64u
			);
		}

		// 見た目の重さを抑えるため、視点はRenderTickで即時反映する。
		if (!mCameraRotator) {
			TryBindCameraRotator();
		}
		if (!mCameraRotator) {
			return;
		}

		const Vec2 mouseLookDelta   = mInput->Axis2D("Mouse");
		const Vec2 gamepadLookDelta = mInput->Axis2D("GamepadLook");
		mCameraRotator->AddLookInput(
			mouseLookDelta,
			gamepadLookDelta,
			std::max(0.0f, frameDeltaTime)
		);
		const Vec2 lookDegrees = mCameraRotator->GetLookAnglesDegrees();
		mLastViewPitchDeg      = lookDegrees.x;
		mLastViewYawDeg        = lookDegrees.y;
	}

	std::string_view PlayerCharacterController::GetStableName() const {
		return "game.PlayerCharacterController";
	}

	std::string_view PlayerCharacterController::GetComponentName() const {
		return "PlayerCharacterController";
	}

	void PlayerCharacterController::TryBindCameraRotator() {
		if (mCameraRotator) {
			return;
		}

		TransformComponent* controlledTransform = nullptr;
		if (Entity* owner = GetOwner()) {
			controlledTransform = owner->GetComponent<TransformComponent>();
		}

		if (auto* scene = GetScene()) {
			for (const auto& entityPtr : scene->GetEntities()) {
				if (!entityPtr) {
					continue;
				}

				auto* rotator = entityPtr->GetComponent<CameraRotatorComponent>();
				if (!rotator) {
					continue;
				}

				auto* rotatorTransform =
					entityPtr->GetComponent<TransformComponent>();
				if (!rotatorTransform) {
					continue;
				}
				if (controlledTransform &&
				    rotatorTransform->Parent() != controlledTransform) {
					continue;
				}

				mCameraRotator = rotator;
				mCameraRotator->SetDirectInputEnabled(false);
				return;
			}
		}
	}

	MovementFrameInput PlayerCharacterController::BuildMovementFrameInput() {
		MovementFrameInput input = {};
		if (!mInput) {
			return input;
		}

		if (mInput->IsHeld("moveright")) {
			input.moveAxis.x += 1.0f;
		}
		if (mInput->IsHeld("moveleft")) {
			input.moveAxis.x -= 1.0f;
		}
		if (mInput->IsHeld("forward")) {
			input.moveAxis.z += 1.0f;
		}
		if (mInput->IsHeld("back")) {
			input.moveAxis.z -= 1.0f;
		}
		if (mInput->IsHeld("moveup")) {
			input.moveAxis.y += 1.0f;
		}
		if (mInput->IsHeld("movedown")) {
			input.moveAxis.y -= 1.0f;
		}

		input.jumpPressed   = mInput->IsHeld("jump");
		input.crouchPressed = mInput->IsHeld("duck");
		input.sprintPressed = mQueuedSprintPressCount > 0;
		if (input.sprintPressed) {
			--mQueuedSprintPressCount;
		}

		const Vec2 gamepadMoveAxis = mInput->Axis2D("GamepadMove");
		input.moveAxis.x += gamepadMoveAxis.x;
		input.moveAxis.z += gamepadMoveAxis.y;

		if (input.moveAxis.Length() > 1.0f) {
			input.moveAxis.Normalize();
		}

		if (const auto* world = GetWorld()) {
			const auto& cameraMgr = world->GetCameraManager();
			const auto  info      = cameraMgr.GetCurrentCameraInfo();
			if (info.valid) {
				input.right   = info.camera.view.Inverse().GetRight();
				input.up      = info.camera.view.Inverse().GetUp();
				input.forward = info.camera.view.Inverse().GetForward();
			}
		}

		return input;
	}

	DemoTickCommand PlayerCharacterController::BuildPlayerTickCommand(
		const uint64_t tick
	) {
		DemoTickCommand command      = {};
		command.tick                 = tick;
		command.subjectEntityGuid    = GetOwner() ? GetOwner()->GetGuid() : 0;
		command.commandType          = DEMO_COMMAND_TYPE::PLAYER_INPUT;
		command.playerInput.movement = BuildMovementFrameInput();
		command.playerInput.viewYawDeg   = mLastViewYawDeg;
		command.playerInput.viewPitchDeg = mLastViewPitchDeg;
		return command;
	}

	void PlayerCharacterController::ApplyLookFromCommand(
		const DemoTickCommand& command, const float stepSeconds
	) {
		(void)stepSeconds;
		if (!mCameraRotator) {
			TryBindCameraRotator();
		}
		if (!mCameraRotator) {
			return;
		}

		mCameraRotator->SetLookAnglesDegrees(
			command.playerInput.viewPitchDeg,
			command.playerInput.viewYawDeg
		);
		mLastViewPitchDeg = command.playerInput.viewPitchDeg;
		mLastViewYawDeg   = command.playerInput.viewYawDeg;
	}

	void PlayerCharacterController::WriteReplayState(
		nlohmann::json& outState
	) const {
		outState["fixedTickCounter"] = mFixedTickCounter;
		outState["lastViewYawDeg"]   = mLastViewYawDeg;
		outState["lastViewPitchDeg"] = mLastViewPitchDeg;
	}

	void PlayerCharacterController::ReadReplayState(const nlohmann::json& inState) {
		if (!inState.is_object()) {
			return;
		}
		mFixedTickCounter = inState.value("fixedTickCounter", mFixedTickCounter);
		mLastViewYawDeg   = inState.value("lastViewYawDeg", mLastViewYawDeg);
		mLastViewPitchDeg = inState.value("lastViewPitchDeg", mLastViewPitchDeg);
	}

	uint64_t PlayerCharacterController::ComputeReplayStateHash() const {
		uint64_t hash = ReplayHash::Begin();
		ReplayHash::AppendPod(hash, mFixedTickCounter);
		ReplayHash::AppendFloating(hash, mLastViewYawDeg);
		ReplayHash::AppendFloating(hash, mLastViewPitchDeg);
		return hash;
	}

#ifdef _DEBUG
	void PlayerCharacterController::DrawInspectorImGui() {
		ImGui::BeginDisabled();
		ImGui::TextUnformatted("PlayerInput");
		ImGuiWidgets::DragVec3(
			"Move Axis", mDebugMoveFrameInput.moveAxis, Vec3::zero, 0.1f, "%.3f"
		);
		ImGuiWidgets::DragVec3(
			"Right", mDebugMoveFrameInput.right, Vec3::zero, 0.1f, "%.3f"
		);
		ImGuiWidgets::DragVec3(
			"Up", mDebugMoveFrameInput.up, Vec3::zero, 0.1f, "%.3f"
		);
		ImGuiWidgets::DragVec3(
			"Forward", mDebugMoveFrameInput.forward, Vec3::zero, 0.1f, "%.3f"
		);
		ImGui::Checkbox("Jump", &mDebugMoveFrameInput.jumpPressed);
		ImGui::Checkbox("Sprint", &mDebugMoveFrameInput.sprintPressed);
		ImGui::Checkbox("Crouch", &mDebugMoveFrameInput.crouchPressed);
		ImGui::EndDisabled();
	}
#endif

	REGISTER_COMPONENT(PlayerCharacterController);
}
