#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include "core/math/Vec2.h"

namespace Unnamed {
	class PortalComponent final : public BaseComponent {
	public:
		void SetExitEntityGuid(uint64_t guid) noexcept;
		[[nodiscard]] uint64_t GetExitEntityGuid() const noexcept;

		void SetSize(const Vec2& size) noexcept;
		[[nodiscard]] Vec2 GetSize() const noexcept;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;
		
		[[nodiscard]] uint32_t GetIcon() const override;

	private:
		uint64_t mExitEntityGuid = 0;
		Vec2     mSize           = Vec2::one;
	};
}
