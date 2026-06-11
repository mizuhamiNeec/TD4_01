#include "GamePause.h"

#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>

#include <engine/unnamed/subsystem/console/Log.h>
#include <engine/unnamed/subsystem/input/InputSystem.h>

namespace Unnamed {
	std::string_view GamePause::GetStableName() const {
		return "mygame.GamePause";
	}

	std::string_view GamePause::GetComponentName() const {
		return "GamePause";
	}

	void GamePause::Deserialize(const JsonReader& reader) {
		mPauseCanvasGuid =
			reader.ReadUint64("pauseCanvasGuid").value_or(
				mPauseCanvasGuid
			);

		mSettingsCanvasGuid =
			reader.ReadUint64("settingsCanvasGuid").value_or(
				mSettingsCanvasGuid
			);
	}

	void GamePause::Serialize(JsonWriter& writer) const {
		writer.Key("pauseCanvasGuid");
		writer.Write(mPauseCanvasGuid);
		writer.Key("settingsCanvasGuid");
		writer.Write(mSettingsCanvasGuid);
	}

	void GamePause::OnAttached() {
		mConsole   = GetConsoleSystem();
		mInput     = GetInputSystem();
		mTimeScale = mConsole ?
			             mConsole->GetConVarAs<ConVar<float>>(
				             "host_timescale"
			             ) :
			             nullptr;
	}

	// OnFrameInputTickはゲーム内時間に影響されない
	void GamePause::OnFrameInputTick(float frameDeltaTime) {
		// 入力とコンソールが取得できないのであれば何もしない
		if (!mInput || !mConsole) {
			return;
		}

		// 入力を取る
		if (mInput->IsPressed("togglepause")) {
			mIsPaused = !mIsPaused;
		}

		// 時間を止めるンゴ
		if (mIsPaused) {
			mTimeScale->SetValue(0.0f);
		} else {
			mTimeScale->SetValue(1.0f);
		}
	}

	void GamePause::DrawInspectorImGui() {
		BaseComponent::DrawInspectorImGui();
	}

	BaseComponent::TICK_GROUP GamePause::GetTickGroup() const {
		return TICK_GROUP::EARLY;
	}
}
