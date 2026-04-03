#pragma once

#include <string_view>
#include <vector>

namespace Unnamed {
	class JsonReader;
	class JsonWriter;
}

namespace Unnamed::Gui {
	struct UiDrawCommand;
	class UiWidget;

	class UiComponent {
	public:
		virtual ~UiComponent() = default;

		[[nodiscard]] virtual std::string_view GetTypeName() const = 0;

		virtual void OnAttached(UiWidget& owner);
		virtual void OnDetached(UiWidget& owner);
		virtual void OnTick(UiWidget& owner, float deltaTime);
		virtual void OnBeforeLayout(UiWidget& owner);
		virtual void OnAfterLayout(UiWidget& owner);
		virtual void BuildDrawCommands(
			const UiWidget& owner, std::vector<UiDrawCommand>& out
		) const;
		virtual void OnMouseEnter(UiWidget& owner);
		virtual void OnMouseLeave(UiWidget& owner);
		virtual void OnMouseDown(UiWidget& owner);
		virtual void OnMouseUp(UiWidget& owner);
		virtual void OnClick(UiWidget& owner);

		virtual void Serialize(JsonWriter& writer) const;
		virtual void Deserialize(const JsonReader& reader);
	};
}
