#include "ParkourMovementComponent.h"

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"

// ReSharper disable once CppUnusedIncludeDirective
#include "game/core/collision/kinematic/BoxKinematicCollisionResolver.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"

#include "state/ParkourMovementStates.h"


namespace Unnamed {
	ParkourMovementComponent::~ParkourMovementComponent() = default;

	std::string_view ParkourMovementComponent::GetStableName() const {
		return "parkour.ParkourMovement";
	}

	std::string_view ParkourMovementComponent::GetComponentName() const {
		return "ParkourMovement";
	}

#ifdef _DEBUG
	void ParkourMovementComponent::DrawInspectorImGui() {
		GameMovementComponent::DrawInspectorImGui();

		ImGui::DragFloat(
			"Parkour Jump Velocity (HU/s)", &mParkourJumpVelocityHu, 1.0f, 0.0f,
			1500.0f
		);
		ImGui::DragFloat(
			"Slide Enter Speed (HU/s)", &mSlideEnterSpeedHu, 1.0f, 0.0f, 1500.0f
		);
		ImGui::DragFloat(
			"Slide Exit Speed (HU/s)", &mSlideExitSpeedHu, 1.0f, 0.0f, 1500.0f
		);
		ImGui::DragFloat(
			"Slide Boost (HU/s)", &mSlideBoostHu, 1.0f, 0.0f, 1500.0f
		);
		ImGui::DragFloat(
			"Slide Friction Scale", &mSlideFrictionScale, 0.01f, 0.01f, 1.0f
		);
		ImGui::DragFloat(
			"Slide Steer Accel Rate", &mSlideSteerAccelRate, 0.01f, 0.0f, 1.0f
		);
		ImGui::DragFloat(
			"Slide Max Duration (s)", &mSlideMaxDurationSec, 0.01f, 0.05f, 5.0f
		);
	}
#endif

	void ParkourMovementComponent::Deserialize(const JsonReader& reader) {
		GameMovementComponent::Deserialize(reader);

		const JsonReader jumpVelocity = reader["parkourJumpVelocityHu"];
		if (jumpVelocity.Valid()) {
			mParkourJumpVelocityHu = jumpVelocity.GetFloat();
		}

		const JsonReader slideEnterSpeed = reader["slideEnterSpeedHu"];
		if (slideEnterSpeed.Valid()) {
			mSlideEnterSpeedHu = slideEnterSpeed.GetFloat();
		}

		const JsonReader slideExitSpeed = reader["slideExitSpeedHu"];
		if (slideExitSpeed.Valid()) {
			mSlideExitSpeedHu = slideExitSpeed.GetFloat();
		}

		const JsonReader slideBoost = reader["slideBoostHu"];
		if (slideBoost.Valid()) {
			mSlideBoostHu = slideBoost.GetFloat();
		}

		const JsonReader slideFrictionScale = reader["slideFrictionScale"];
		if (slideFrictionScale.Valid()) {
			mSlideFrictionScale = slideFrictionScale.GetFloat();
		}

		const JsonReader slideSteerAccelRate = reader["slideSteerAccelRate"];
		if (slideSteerAccelRate.Valid()) {
			mSlideSteerAccelRate = slideSteerAccelRate.GetFloat();
		}

		const JsonReader slideMaxDurationSec = reader["slideMaxDurationSec"];
		if (slideMaxDurationSec.Valid()) {
			mSlideMaxDurationSec = slideMaxDurationSec.GetFloat();
		}
	}

	void ParkourMovementComponent::Serialize(JsonWriter& writer) const {
		GameMovementComponent::Serialize(writer);

		writer.Key("parkourJumpVelocityHu");
		writer.Write(mParkourJumpVelocityHu);
		writer.Key("slideEnterSpeedHu");
		writer.Write(mSlideEnterSpeedHu);
		writer.Key("slideExitSpeedHu");
		writer.Write(mSlideExitSpeedHu);
		writer.Key("slideBoostHu");
		writer.Write(mSlideBoostHu);
		writer.Key("slideFrictionScale");
		writer.Write(mSlideFrictionScale);
		writer.Key("slideSteerAccelRate");
		writer.Write(mSlideSteerAccelRate);
		writer.Key("slideMaxDurationSec");
		writer.Write(mSlideMaxDurationSec);
	}

	void ParkourMovementComponent::RegisterMovementStates(
		GameMovementStateMachine& stateMachine
	) {
		GameMovementComponent::RegisterMovementStates(stateMachine);

		const ParkourMovementTuning tuning = {
			.jumpVelocityHu      = mParkourJumpVelocityHu,
			.slideEnterSpeedHu   = mSlideEnterSpeedHu,
			.slideExitSpeedHu    = mSlideExitSpeedHu,
			.slideBoostHu        = mSlideBoostHu,
			.slideFrictionScale  = mSlideFrictionScale,
			.slideSteerAccelRate = mSlideSteerAccelRate,
			.slideMaxDurationSec = mSlideMaxDurationSec
		};
		RegisterParkourMovementStates(stateMachine, tuning);
	}

	REGISTER_COMPONENT(ParkourMovementComponent);
}
