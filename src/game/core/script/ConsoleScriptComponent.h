#pragma once
#include <string>

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class ConsoleScriptComponent : public BaseComponent {
	public:
		void OnAttached() override;
		void OnDetached() override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

		[[nodiscard]] uint32_t GetIcon() const override;

#ifdef	_DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		std::string mOnAttachedScriptPath;
		std::string mOnDetachedScriptPath;
	};
}
