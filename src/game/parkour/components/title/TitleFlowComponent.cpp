#include "TitleFlowComponent.h"

#include <algorithm>
#include <cmath>

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/ImGui/Icons.h"
#include "engine/gui/UiRoot.h"
#include "engine/gui/UiWidget.h"
#include "engine/gui/components/UiButtonBehaviorComponent.h"
#include "engine/gui/components/UiTextureComponent.h"
#include "engine/unnamed/framework/components/ui/UiCanvasComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/world/World.h"
#include "game/core/replay/DemoManager.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "TitleFlow";

		constexpr const char* kTitleModeCvar         = "cl_title_mode";
		constexpr const char* kTitleDemoPathCvar     = "cl_title_demo_path";
		constexpr const char* kDemoMismatchPolicyCvar = "demo_mismatch_policy";
		constexpr const char* kTitleFadeOutSecCvar   = "cl_title_fade_out_sec";
		constexpr const char* kTitleFadeInSecCvar    = "cl_title_fade_in_sec";
		constexpr const char* kTitleStartFadeSecCvar =
			"cl_title_start_transition_sec";
		constexpr const char* kDefaultTitleDemoPath =
			"./content/parkour/replay/title_demo.udemo";
		constexpr float kDemoRetryIntervalSec = 0.75f;

		float EvaluateFadeEase(const float t) {
			return Math::CubicBezier(
				std::clamp(t, 0.0f, 1.0f),
				0.2f,
				0.0f,
				0.0f,
				1.0f
			);
		}
	}

	void TitleFlowComponent::OnTick(const float deltaTime) {
		TickTitle(deltaTime);
	}

	void TitleFlowComponent::OnEditorTick(const float deltaTime) {
		TickTitle(deltaTime);
	}

	void TitleFlowComponent::OnDetached() {
		ExitTitleMode();
		BaseComponent::OnDetached();
	}

	std::string_view TitleFlowComponent::GetStableName() const {
		return "parkour.TitleFlow";
	}

	std::string_view TitleFlowComponent::GetComponentName() const {
		return "TitleFlow";
	}

	uint32_t TitleFlowComponent::GetIcon() const {
		return kIconDesktopLandscape;
	}

	void TitleFlowComponent::Deserialize(const JsonReader& reader) {
		mGameplayScenePath = reader["gameplayScenePath"].GetString(mGameplayScenePath);
		mPlayButtonName    = reader["playButtonName"].GetString(mPlayButtonName);
		mQuitButtonName    = reader["quitButtonName"].GetString(mQuitButtonName);
		mPlayPromptName    = reader["playPromptName"].GetString(mPlayPromptName);
		mFadeOverlayName   = reader["fadeOverlayName"].GetString(mFadeOverlayName);
	}

	void TitleFlowComponent::Serialize(JsonWriter& writer) const {
		writer.Key("gameplayScenePath");
		writer.Write(mGameplayScenePath);
		writer.Key("playButtonName");
		writer.Write(mPlayButtonName);
		writer.Key("quitButtonName");
		writer.Write(mQuitButtonName);
		writer.Key("playPromptName");
		writer.Write(mPlayPromptName);
		writer.Key("fadeOverlayName");
		writer.Write(mFadeOverlayName);
	}

	void TitleFlowComponent::TickTitle(const float deltaTime) {
		const bool titleEnabled = IsTitleModeEnabled();

		// タイトルモードの切り替え境界で初期化/後始末を行います。
		if (!titleEnabled) {
			if (mInitialized) {
				ExitTitleMode();
			}
			return;
		}
		if (!mInitialized) {
			EnterTitleMode();
		}

		ResolveUiBindings();
		ApplyUiVisibility();

		if (mQuitRequested) {
			if (ConsoleSystem* console = GetConsoleSystem()) {
				console->ExecuteCommand("quit");
			}
			mQuitRequested = false;
		}

		if (!mPlayRequested) {
			if (const InputSystem* input = GetInputSystem()) {
				if (input->IsPressed("jump")) {
					mPlayRequested = true;
				}
			}
		}
		if (mPlayRequested && mPhase != PHASE::START_TRANSITION_OUT) {
			mPhase               = PHASE::START_TRANSITION_OUT;
			mPhaseElapsedSeconds = 0.0f;
		}

		UpdatePhase(deltaTime);
		UpdatePlayPromptBlink(deltaTime);
	}

	bool TitleFlowComponent::IsTitleModeEnabled() const {
		ConsoleSystem* console = GetConsoleSystem();
		if (!console) {
			return false;
		}
		return console->GetConVarValueOr<int>(kTitleModeCvar, 0) == 1;
	}

	void TitleFlowComponent::EnterTitleMode() {
		mInitialized         = true;
		mPlayRequested       = false;
		mQuitRequested       = false;
		mDemoRetrySeconds    = 0.0f;
		mHasSavedMismatchPolicy = false;
		mSavedMismatchPolicy.clear();
		mPhase               = PHASE::IDLE;
		mPhaseElapsedSeconds = 0.0f;
		mPromptBlinkSeconds  = 0.0f;

		if (Entity* owner = GetOwner()) {
			owner->SetVisible(true);
		}
		if (ConsoleSystem* console = GetConsoleSystem()) {
			// タイトル再生中はミスマッチ停止ループを防ぐため、継続ポリシーを強制します。
			mSavedMismatchPolicy = console->GetConVarValueString(
				kDemoMismatchPolicyCvar
			);
			mHasSavedMismatchPolicy = !mSavedMismatchPolicy.empty();
			console->ExecuteCommand("demo_mismatch_policy continue");
		}

		ResolveUiBindings();
		(void)RequestDemoPlayback();
		SetFadeOverlayAlpha(0.0f);
	}

	void TitleFlowComponent::ExitTitleMode() {
		mInitialized         = false;
		mPlayRequested       = false;
		mQuitRequested       = false;
		mDemoRetrySeconds    = 0.0f;
		if (mHasSavedMismatchPolicy) {
			if (ConsoleSystem* console = GetConsoleSystem()) {
				console->ExecuteCommand(
					"demo_mismatch_policy " + mSavedMismatchPolicy
				);
			}
		}
		mHasSavedMismatchPolicy = false;
		mSavedMismatchPolicy.clear();
		mPhase               = PHASE::INACTIVE;
		mPhaseElapsedSeconds = 0.0f;
		mPromptBlinkSeconds  = 0.0f;
		mActiveDemoPath.clear();

		if (Entity* owner = GetOwner()) {
			owner->SetVisible(false);
		}
		if (mUiCanvas) {
			mUiCanvas->SetReceiveInput(false);
		}
	}

	bool TitleFlowComponent::RequestDemoPlayback() {
		DemoManager* demo = GetDemoManager();
		if (!demo) {
			return false;
		}

		const std::string demoPath = ResolveDemoPath();
		if (demoPath.empty()) {
			Warning(kChannel, "Title demo path is empty.");
			mDemoRetrySeconds = kDemoRetryIntervalSec;
			return false;
		}

		// 同じdemoが既に再生中なら再起動せず、そのまま維持します。
		if (demo->IsPlayback() && demo->GetCurrentPath() == demoPath) {
			return true;
		}

		// StartPlaybackの成否を直接参照し、失敗時の毎フレーム再実行を防ぎます。
		if (!demo->StartPlayback(demoPath)) {
			Warning(
				kChannel,
				"Failed to start title demo playback. path='{}'",
				demoPath
			);
			mDemoRetrySeconds = kDemoRetryIntervalSec;
			return false;
		}

		mDemoRetrySeconds = 0.0f;
		mActiveDemoPath = demoPath;
		return true;
	}

	void TitleFlowComponent::UpdatePhase(const float deltaTime) {
		DemoManager* demo = GetDemoManager();

		// 再生開始に失敗した場合は短い間隔で再試行します。
		mDemoRetrySeconds = std::max(0.0f, mDemoRetrySeconds - deltaTime);

		// タイトル待機中のみ、停止状態ならクールダウン後に再試行します。
		if (mPhase == PHASE::IDLE &&
		    demo &&
		    !demo->IsPlayback() &&
		    !demo->IsRecording() &&
		    mDemoRetrySeconds <= 0.0f) {
			(void)RequestDemoPlayback();
		}

		switch (mPhase) {
			case PHASE::IDLE: {
				SetFadeOverlayAlpha(0.0f);
				if (demo && demo->IsPlaybackFinished()) {
					mPhase               = PHASE::DEMO_LOOP_FADE_OUT;
					mPhaseElapsedSeconds = 0.0f;
				}
				break;
			}
			case PHASE::DEMO_LOOP_FADE_OUT: {
				mPhaseElapsedSeconds += deltaTime;
				const float duration = GetSecondsCvar(
					kTitleFadeOutSecCvar,
					0.22f,
					0.01f
				);
				const float t = std::clamp(
					mPhaseElapsedSeconds / duration,
					0.0f,
					1.0f
				);
				SetFadeOverlayAlpha(EvaluateFadeEase(t));
				if (t >= 1.0f) {
					if (RequestDemoPlayback()) {
						mPhase               = PHASE::DEMO_LOOP_FADE_IN;
						mPhaseElapsedSeconds = 0.0f;
					} else {
						mPhase               = PHASE::IDLE;
						mPhaseElapsedSeconds = 0.0f;
						SetFadeOverlayAlpha(0.0f);
					}
				}
				break;
			}
			case PHASE::DEMO_LOOP_FADE_IN: {
				mPhaseElapsedSeconds += deltaTime;
				const float duration = GetSecondsCvar(
					kTitleFadeInSecCvar,
					0.24f,
					0.01f
				);
				const float t = std::clamp(
					mPhaseElapsedSeconds / duration,
					0.0f,
					1.0f
				);
				SetFadeOverlayAlpha(1.0f - EvaluateFadeEase(t));
				if (t >= 1.0f) {
					mPhase               = PHASE::IDLE;
					mPhaseElapsedSeconds = 0.0f;
					SetFadeOverlayAlpha(0.0f);
				}
				break;
			}
			case PHASE::START_TRANSITION_OUT: {
				mPhaseElapsedSeconds += deltaTime;
				const float duration = GetSecondsCvar(
					kTitleStartFadeSecCvar,
					0.30f,
					0.01f
				);
				const float t = std::clamp(
					mPhaseElapsedSeconds / duration,
					0.0f,
					1.0f
				);
				SetFadeOverlayAlpha(EvaluateFadeEase(t));
				if (t >= 1.0f) {
					CommitStartTransition();
				}
				break;
			}
			case PHASE::INACTIVE:
			default: break;
		}
	}

	void TitleFlowComponent::CommitStartTransition() {
		ConsoleSystem* console = GetConsoleSystem();
		DemoManager*   demo    = GetDemoManager();
		World*         world   = GetWorld();
		if (!console || !world) {
			return;
		}

		// シーン遷移前に再生を同期停止し、次シーンへの状態持ち越しを防ぎます。
		if (demo && (demo->IsPlayback() || demo->IsRecording())) {
			(void)demo->Stop();
		}

		// 遷移後にタイトル演出が再実行されないようモードを先に戻します。
		console->ExecuteCommand("cl_title_mode 0");
		world->RequestSceneTransition(mGameplayScenePath);
	}

	void TitleFlowComponent::ResolveUiBindings() {
		mUiCanvas = nullptr;
		Entity* owner = GetOwner();
		if (!owner) {
			return;
		}

		mUiCanvas = owner->GetComponent<UiCanvasComponent>();
		if (!mUiCanvas || !mUiCanvas->EnsureRuntimeLoaded()) {
			return;
		}

		Gui::UiRoot* root = mUiCanvas->GetRuntimeRoot();
		Gui::UiWidget* rootWidget = root ? root->GetRootWidget() : nullptr;
		if (!rootWidget) {
			return;
		}

		mPlayButtonWidget = FindWidgetByNameRecursive(rootWidget, mPlayButtonName);
		mQuitButtonWidget = FindWidgetByNameRecursive(rootWidget, mQuitButtonName);
		Gui::UiWidget* playPromptWidget = FindWidgetByNameRecursive(
			rootWidget,
			mPlayPromptName
		);
		Gui::UiWidget* fadeOverlayWidget = FindWidgetByNameRecursive(
			rootWidget,
			mFadeOverlayName
		);

		mPlayPromptTexture = playPromptWidget ?
			                     playPromptWidget->GetOrAddComponent<Gui::UiTextureComponent>() :
			                     nullptr;
		mFadeOverlayTexture = fadeOverlayWidget ?
			                      fadeOverlayWidget->GetOrAddComponent<Gui::UiTextureComponent>() :
			                      nullptr;

		if (mPlayButtonWidget) {
			if (auto* playBehavior = mPlayButtonWidget->GetComponent<Gui::UiButtonBehaviorComponent>()) {
				playBehavior->SetOnClick([this]() { RequestStart(); });
			}
		}
		if (mQuitButtonWidget) {
			if (auto* quitBehavior = mQuitButtonWidget->GetComponent<Gui::UiButtonBehaviorComponent>()) {
				quitBehavior->SetOnClick([this]() { RequestQuit(); });
			}
		}
	}

	Gui::UiWidget* TitleFlowComponent::FindWidgetByNameRecursive(
		Gui::UiWidget* root,
		const std::string_view widgetName
	) {
		if (!root) {
			return nullptr;
		}
		if (root->GetName() == widgetName) {
			return root;
		}

		for (const auto& child : root->GetChildren()) {
			if (!child) {
				continue;
			}
			if (Gui::UiWidget* found = FindWidgetByNameRecursive(child.get(), widgetName)) {
				return found;
			}
		}
		for (Gui::UiWidget* child : root->GetReferenceChildren()) {
			if (Gui::UiWidget* found = FindWidgetByNameRecursive(child, widgetName)) {
				return found;
			}
		}
		return nullptr;
	}

	void TitleFlowComponent::ApplyUiVisibility() const {
		if (Entity* owner = GetOwner()) {
			owner->SetVisible(mInitialized);
		}
		if (mUiCanvas) {
			mUiCanvas->SetReceiveInput(mInitialized);
		}
	}

	void TitleFlowComponent::SetFadeOverlayAlpha(const float alpha) const {
		if (!mFadeOverlayTexture) {
			return;
		}
		Gui::Color color = mFadeOverlayTexture->GetColor();
		color.a          = std::clamp(alpha, 0.0f, 1.0f);
		mFadeOverlayTexture->SetColor(color);
	}

	void TitleFlowComponent::UpdatePlayPromptBlink(const float deltaTime) {
		if (!mPlayPromptTexture) {
			return;
		}

		// Idle中のみ点滅させて、遷移時は視認性を優先して固定表示にします。
		if (mPhase != PHASE::IDLE) {
			Gui::Color fixedColor = mPlayPromptTexture->GetColor();
			fixedColor.a          = 1.0f;
			mPlayPromptTexture->SetColor(fixedColor);
			return;
		}

		mPromptBlinkSeconds += deltaTime;

		const float blink = 0.5f + 0.5f * std::sin(mPromptBlinkSeconds * 4.0f);
		Gui::Color color  = mPlayPromptTexture->GetColor();
		color.a           = 0.45f + 0.55f * blink;
		mPlayPromptTexture->SetColor(color);
	}

	float TitleFlowComponent::GetSecondsCvar(
		const std::string_view name,
		const float            fallback,
		const float            minValue
	) const {
		ConsoleSystem* console = GetConsoleSystem();
		const float value = console ?
			                    console->GetConVarValueOr<float>(name, fallback) :
			                    fallback;
		return std::max(minValue, value);
	}

	std::string TitleFlowComponent::ResolveDemoPath() const {
		ConsoleSystem* console = GetConsoleSystem();
		std::string path = console ?
			                   console->GetConVarValueString(kTitleDemoPathCvar) :
			                   std::string();
		if (path.empty()) {
			path = kDefaultTitleDemoPath;
		}
		return path;
	}

	void TitleFlowComponent::RequestStart() {
		mPlayRequested = true;
	}

	void TitleFlowComponent::RequestQuit() {
		mQuitRequested = true;
	}

	REGISTER_COMPONENT(TitleFlowComponent);
}
