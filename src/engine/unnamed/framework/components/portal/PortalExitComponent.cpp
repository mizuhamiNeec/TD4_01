#include "PortalExitComponent.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

namespace Unnamed {
	std::string_view PortalExitComponent::GetStableName() const {
		return "engine.PortalExitComponent";
	}

	std::string_view PortalExitComponent::GetComponentName() const {
		return "PortalExit";
	}

#ifdef _DEBUG
	void PortalExitComponent::DrawInspectorImGui() {
		BaseComponent::DrawInspectorImGui();
		ImGui::TextWrapped("This component marks the entity as a portal exit.");
	}
#endif

	void PortalExitComponent::Deserialize(const JsonReader& reader) {
		(void)reader;
	}

	void PortalExitComponent::Serialize(JsonWriter& writer) const {
		(void)writer;
	}

	uint32_t PortalExitComponent::GetIcon() const {
		return 0; // Use default or specific icon if available
	}
}
