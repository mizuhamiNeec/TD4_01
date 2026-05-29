#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class PortalExitComponent final : public BaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;
		
		[[nodiscard]] uint32_t GetIcon() const override;
	};
}
