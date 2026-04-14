#include "CheckpointComponent.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

namespace Unnamed {
	void CheckpointComponent::Deserialize(const JsonReader& reader) {
		DeserializeVolume(reader);
		const JsonReader index = reader["index"];
		if (index.Valid()) {
			mIndex = index.GetInt();
		}

		mRespawnPosition = reader["respawnPosition"].GetVec3(mRespawnPosition);
	}

	void CheckpointComponent::Serialize(JsonWriter& writer) const {
		SerializeVolume(writer);
		writer.Key("index");
		writer.Write(mIndex);
		writer.WriteVec3("respawnPosition", mRespawnPosition);
	}

#ifdef _DEBUG
	void CheckpointComponent::DrawInspectorImGui() {
		DrawVolumeInspectorImGui();
		ImGui::InputInt("Index", &mIndex);
		ImGui::DragFloat3("Respawn Position", &mRespawnPosition.x, 1.0f);
	}
#endif

	REGISTER_COMPONENT(CheckpointComponent);
}
