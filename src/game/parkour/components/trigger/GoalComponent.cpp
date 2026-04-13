#include "GoalComponent.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

namespace Unnamed {
	void GoalComponent::Deserialize(const JsonReader& reader) {
		DeserializeVolume(reader);
		const JsonReader index = reader["index"];
		if (index.Valid()) {
			mIndex = index.GetInt();
		}
	}

	void GoalComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
		writer.Key("index");
		writer.Write(mIndex);
	}

#ifdef _DEBUG
	void GoalComponent::DrawInspectorImGui() {
		DrawVolumeInspectorImGui();
		ImGui::InputInt("Index", &mIndex);
	}
#endif

	REGISTER_COMPONENT(GoalComponent);
}
