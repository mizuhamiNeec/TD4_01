#include "PortalComponent.h"

#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/scene/Scene.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

namespace Unnamed {
	void PortalComponent::SetExitEntityGuid(uint64_t guid) noexcept {
		mExitEntityGuid = guid;
	}

	uint64_t PortalComponent::GetExitEntityGuid() const noexcept {
		return mExitEntityGuid;
	}

	void PortalComponent::SetSize(const Vec2& size) noexcept {
		mSize = size;
	}

	Vec2 PortalComponent::GetSize() const noexcept {
		return mSize;
	}

	std::string_view PortalComponent::GetStableName() const {
		return "engine.PortalComponent";
	}

	std::string_view PortalComponent::GetComponentName() const {
		return "Portal";
	}

#ifdef _DEBUG
	void PortalComponent::DrawInspectorImGui() {
		BaseComponent::DrawInspectorImGui();

		float size[2] = { mSize.x, mSize.y };
		if (ImGui::DragFloat2("Size", size, 0.1f, 0.0f)) {
			mSize.x = size[0];
			mSize.y = size[1];
		}

		uint64_t targetGuid = mExitEntityGuid;
		if (ImGui::InputScalar("Exit Entity GUID", ImGuiDataType_U64, &targetGuid)) {
			mExitEntityGuid = targetGuid;
		}
	}
#endif

	void PortalComponent::Deserialize(const JsonReader& reader) {
		mExitEntityGuid = reader.ReadUint64("ExitEntityGuid").value_or(0ull);
		mSize.x = reader.Read<float>("SizeX").value_or(1.0f);
		mSize.y = reader.Read<float>("SizeY").value_or(1.0f);
	}

	void PortalComponent::Serialize(JsonWriter& writer) const {
		writer.Key("ExitEntityGuid");
		writer.Write(mExitEntityGuid);
		writer.Key("SizeX");
		writer.Write(mSize.x);
		writer.Key("SizeY");
		writer.Write(mSize.y);
	}

	uint32_t PortalComponent::GetIcon() const {
		return 0;
	}
}
