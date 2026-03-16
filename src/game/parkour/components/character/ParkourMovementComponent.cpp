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

	void ParkourMovementComponent::OnAttached() {
		GameMovementComponent::OnAttached();

		if (mConsole) {
			mJumpVelocity = GetConVarSafe<float>(mConsole, "sv_jumpvelocity");
		}
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
		GameMovementComponent::RegisterMovementStates(stateMachine);
		RegisterParkourMovementStates(stateMachine);
	}

	REGISTER_COMPONENT(ParkourMovementComponent);
}
