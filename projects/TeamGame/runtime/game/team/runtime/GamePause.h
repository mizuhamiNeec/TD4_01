#pragma once
#include <engine/unnamed/framework/components/base/BaseComponent.h>
#include <engine/unnamed/subsystem/console/concommand/ConVar.h>

namespace Unnamed {
	class GamePause : public BaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;
		void OnAttached() override;
		void OnFrameInputTick(float frameDeltaTime) override;
#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif
		[[nodiscard]] TICK_GROUP GetTickGroup() const override;

	private:
		InputSystem* mInput = nullptr;
		ConsoleSystem* mConsole = nullptr;

		ConVar<float>* mTimeScale = nullptr;

		uint64_t mSettingsCanvasGuid = 0u;
		uint64_t mPauseCanvasGuid    = 0u;

		bool mIsPaused = false;
	};
}
