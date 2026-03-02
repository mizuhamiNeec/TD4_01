#pragma once

#include "base/UBaseComponent.h"

#include "core/math/Vec2.h"

namespace Unnamed {
	class JsonReader;
	class JsonWriter;

	class PortalComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "engine.Portal";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Portal";
		}

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void                   SetLinkedPortalGuid(uint64_t guid) noexcept;
		[[nodiscard]] uint64_t GetLinkedPortalGuid() const noexcept;

		void               SetEnabled(bool enabled) noexcept;
		[[nodiscard]] bool IsEnabled() const noexcept;

		void               SetHalfExtents(Vec2 halfExtents) noexcept;
		[[nodiscard]] Vec2 GetHalfExtents() const noexcept;
		void               SetUseAsPortalSurface(bool enabled) noexcept;
		[[nodiscard]] bool GetUseAsPortalSurface() const noexcept;

	private:
		uint64_t mLinkedPortalGuid   = 0;
		bool     mEnabled            = true;
		bool     mUseAsPortalSurface = true;
		Vec2     mHalfExtents        = Vec2(0.5f, 1.0f);
	};
}
