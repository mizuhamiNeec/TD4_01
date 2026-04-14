#include "ConsoleScriptComponent.h"

#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/Icons.h"
#include "engine/unnamed/subsystem/console/ConsoleScriptParser.h"

namespace Unnamed {
	void ConsoleScriptComponent::OnAttached() {
		BaseComponent::OnAttached();

		ConsoleScriptParser parser(mOnAttachedScriptPath);
	}

	void ConsoleScriptComponent::OnDetached() {
		BaseComponent::OnDetached();

		ConsoleScriptParser parser(mOnDetachedScriptPath);
	}

	std::string_view ConsoleScriptComponent::GetStableName() const {
		return "engine.ScriptComponent";
	}

	std::string_view ConsoleScriptComponent::GetComponentName() const {
		return "ScriptComponent";
	}

	uint32_t ConsoleScriptComponent::GetIcon() const {
		return kIconArticle;
	}

	void ConsoleScriptComponent::DrawInspectorImGui() {}

	void ConsoleScriptComponent::Deserialize(const JsonReader& reader) {
		const JsonReader attached = reader["attached"];
		if (attached.Valid()) {
			mOnAttachedScriptPath = attached.GetString();
		}
		const JsonReader detached = reader["detached"];
		if (detached.Valid()) {
			mOnDetachedScriptPath = detached.GetString();
		}
	}

	void ConsoleScriptComponent::Serialize(JsonWriter& writer) const {
		writer.Key("attached");
		writer.Write(mOnAttachedScriptPath);
		writer.Key("detached");
		writer.Write(mOnDetachedScriptPath);
	}
}
