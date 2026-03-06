#include "PortalComponent.h"

#include <algorithm>

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"

namespace Unnamed {
	namespace {
		bool ReadBoolOr(
			const JsonReader& reader, const char* key, const bool fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetBool() : fallback;
		}

		uint64_t ReadU64Or(
			const JsonReader& reader, const char* key, const uint64_t fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetUint64() : fallback;
		}

		Vec2 ReadVec2Or(
			const JsonReader& reader, const char* key, const Vec2 fallback
		) {
			const JsonReader value = reader[key];
			if (!value.Valid() || value.Size() < 2) {
				return fallback;
			}
			return Vec2(value[0].GetFloat(), value[1].GetFloat());
		}
	}

	void PortalComponent::Deserialize(const JsonReader& reader) {
		mLinkedPortalGuid   = ReadU64Or(reader, "linkedPortalGuid", 0);
		mEnabled            = ReadBoolOr(reader, "enabled", true);
		mUseAsPortalSurface = ReadBoolOr(reader, "useAsPortalSurface", true);
		mHalfExtents        = ReadVec2Or(
			reader, "halfExtents", Vec2(0.5f, 1.0f)
		);
	}

	void PortalComponent::Serialize(JsonWriter& writer) const {
		writer.Key("linkedPortalGuid");
		writer.Write(mLinkedPortalGuid);
		writer.Key("enabled");
		writer.Write(mEnabled);
		writer.Key("useAsPortalSurface");
		writer.Write(mUseAsPortalSurface);
		writer.Key("halfExtents");
		writer.BeginArray();
		writer.Write(mHalfExtents.x);
		writer.Write(mHalfExtents.y);
		writer.EndArray();
	}

#ifdef _DEBUG
	void PortalComponent::DrawInspectorImGui() {
		bool enabled = mEnabled;
		if (ImGui::Checkbox("Enabled", &enabled)) {
			mEnabled = enabled;
		}
		bool useAsPortalSurface = mUseAsPortalSurface;
		if (
			ImGui::Checkbox(
				"UseAsPortalSurface",
				&useAsPortalSurface
			)
		) {
			mUseAsPortalSurface = useAsPortalSurface;
		}

		long long linkedGuid = static_cast<long long>(mLinkedPortalGuid);
		if (ImGui::InputScalar(
			"LinkedPortalGuid",
			ImGuiDataType_S64,
			&linkedGuid
		)) {
			mLinkedPortalGuid = linkedGuid > 0 ?
				                    static_cast<uint64_t>(linkedGuid) :
				                    0;
		}

		float halfExtents[2] = {mHalfExtents.x, mHalfExtents.y};
		if (
			ImGui::DragFloat2(
				"HalfExtents",
				halfExtents,
				0.01f,
				0.01f,
				1000.0f
			)
		) {
			mHalfExtents.x = std::max(0.01f, halfExtents[0]);
			mHalfExtents.y = std::max(0.01f, halfExtents[1]);
		}
	}
#endif

	void PortalComponent::SetLinkedPortalGuid(const uint64_t guid) noexcept {
		mLinkedPortalGuid = guid;
	}

	uint64_t PortalComponent::GetLinkedPortalGuid() const noexcept {
		return mLinkedPortalGuid;
	}

	void PortalComponent::SetEnabled(const bool enabled) noexcept {
		mEnabled = enabled;
	}

	bool PortalComponent::IsEnabled() const noexcept {
		return mEnabled;
	}

	void PortalComponent::SetHalfExtents(const Vec2 halfExtents) noexcept {
		mHalfExtents.x = std::max(0.01f, halfExtents.x);
		mHalfExtents.y = std::max(0.01f, halfExtents.y);
	}

	Vec2 PortalComponent::GetHalfExtents() const noexcept {
		return mHalfExtents;
	}

	void PortalComponent::SetUseAsPortalSurface(const bool enabled) noexcept {
		mUseAsPortalSurface = enabled;
	}

	bool PortalComponent::GetUseAsPortalSurface() const noexcept {
		return mUseAsPortalSurface;
	}

	REGISTER_COMPONENT(PortalComponent);
}
