#include "ParkourMovementComponent.h"

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"

// ReSharper disable once CppUnusedIncludeDirective
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/World.h"

#include "game/core/collision/kinematic/BoxKinematicCollisionResolver.h"
#include "game/core/components/character/state/GameMovementStateMachine.h"

#include "state/ParkourMovementStates.h"


namespace Unnamed {
	ParkourMovementComponent::~ParkourMovementComponent() = default;

	void ParkourMovementComponent::OnAttached() {
		GameMovementComponent::OnAttached();
	}

	std::string_view ParkourMovementComponent::GetStableName() const {
		return "parkour.ParkourMovement";
	}

	std::string_view ParkourMovementComponent::GetComponentName() const {
		return "ParkourMovement";
	}

#ifdef _DEBUG
	void ParkourMovementComponent::DrawInspectorImGui() {
		GameMovementComponent::DrawInspectorImGui();

		mOwner->GetWorld()->SetPostFxScalarOverride(
			"Exposure", "Intensity", 100.0f
		);
	}
#endif

	void ParkourMovementComponent::Deserialize(const JsonReader& reader) {
		GameMovementComponent::Deserialize(reader);
	}

	void ParkourMovementComponent::Serialize(JsonWriter& writer) const {
		GameMovementComponent::Serialize(writer);
	}

	void ParkourMovementComponent::RegisterMovementStates(
		GameMovementStateMachine& stateMachine
	) {
		RegisterParkourMovementStates(stateMachine);
		mStateMachine->ChangeState("ParkourAirMove");
	}

	REGISTER_COMPONENT(ParkourMovementComponent);
}
