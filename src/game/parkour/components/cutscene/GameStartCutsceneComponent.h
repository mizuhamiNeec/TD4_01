#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/math/Vec2.h"
#include "core/math/Vec3.h"

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	class AudioSourceComponent;
	class BaseComponent;
	class CameraRotatorComponent;
	class Entity;
	class JsonReader;
	class JsonWriter;
	class TransformComponent;
	class UiCanvasComponent;

	namespace Gui {
		class UiTextureComponent;
		class UiTransformComponent;
		class UiWidget;
	}

	/// @brief Branch版のゲーム開始演出（カメラツアー/カウントダウン/開始解放）を再現するコンポーネントです。
	class GameStartCutsceneComponent final : public BaseComponent {
	public:
		/// @brief コンポーネント追加時に開始演出を初期化します。
		void OnAttached() override;
		/// @brief 開始演出を毎フレーム更新します。
		void OnTick(float deltaTime) override;
		/// @brief エディター時にショット座標のデバッグ表示を更新します。
		void OnEditorTick(float deltaTime) override;
		/// @brief コンポーネント破棄時にロック解除とUI非表示を保証します。
		void OnDetached() override;

		/// @brief stableName を返します。
		[[nodiscard]] std::string_view GetStableName() const override;
		/// @brief 表示用のコンポーネント名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t GetIcon() const override;

#ifdef _DEBUG
		/// @brief インスペクタ用UIを描画します。
		void DrawInspectorImGui() override;
#endif

		/// @brief JSON から設定を読み込みます。
		void Deserialize(const JsonReader& reader) override;
		/// @brief 設定を JSON に書き出します。
		void Serialize(JsonWriter& writer) const override;

	private:
		/// @brief カメラショット定義です。
		struct ShotSpec {
			Vec3  startPos        = Vec3::zero;
			Vec3  endPos          = Vec3::zero;
			Vec3  startLook       = Vec3::forward;
			Vec3  endLook         = Vec3::forward;
			float durationSeconds = 1.0f;
		};

		/// @brief ロック対象定義です。
		struct LockTargetSpec {
			uint64_t    entityGuid          = 0;
			std::string componentStableName = "";
		};

		/// @brief 実行中のロック復帰情報です。
		struct ActiveLockState {
			BaseComponent* component      = nullptr;
			bool           previousActive = true;
		};

		/// @brief 開始演出の実行フェーズです。
		enum class PHASE : uint8_t {
			TOUR = 0,
			COUNTDOWN,
			GAMEPLAY,
		};

		/// @brief 開始演出を初期化して Tour を開始します。
		void StartSequence();
		/// @brief Tour を更新します。
		void TickTour(float deltaTime);
		/// @brief Countdown を更新します。
		void TickCountdown(float deltaTime);
		/// @brief Tour から Countdown に遷移します。
		void EnterCountdown();
		/// @brief 演出完了で Gameplay に遷移します。
		void EnterGameplay();

		/// @brief Countdown のSEトリガーを更新します。
		void UpdateCountdownAudio();
		/// @brief 演出UIの表示状態を更新します。
		void UpdateHudWidgets();
		/// @brief フェードオーバーレイの表示を更新します。
		void UpdateFadeOverlayWidget() const;
		/// @brief 3-2-1-START 表示を更新します。
		void UpdateCountdownWidgets() const;
		/// @brief カウントダウンUIを非表示にします。
		void HideCountdownWidgets() const;
		/// @brief 演出UIをすべて非表示にします。
		void HideAllCutsceneWidgets() const;

		/// @brief カメラ/UI/Audio の参照を再解決します。
		void ResolveBindings();
		/// @brief 解決済み参照のキャッシュを初期化します。
		void ClearResolvedBindings();
		/// @brief lockTargets を無効化して入力/操作を抑止します。
		void ApplyLockTargets();
		/// @brief lockTargets の有効状態を復帰します。
		void RestoreLockTargets();

		/// @brief ショット姿勢を cameraRoot + cameraRotator へ適用します。
		void ApplyCameraPose(const Vec3& cameraPos, const Vec3& lookAtPos);
		/// @brief Countdown固定姿勢を cameraRoot + cameraRotator へ適用します。
		void ApplyLookAnglesAndPosition(
			const Vec3& position,
			const Vec2& lookAngles
		);

		/// @brief lock spec から対象コンポーネントを解決します。
		[[nodiscard]] BaseComponent* ResolveLockTarget(const LockTargetSpec& spec)
		const;
		/// @brief GUID から AudioSource を解決します。
		[[nodiscard]] AudioSourceComponent* ResolveAudioSourceByGuid(
			uint64_t componentGuid
		) const;
		/// @brief 画面解像度を取得します。
		[[nodiscard]] bool ResolveViewportSize(Vec2& outViewportSizePx) const;

		/// @brief ウィジェット名を再帰的に検索します。
		[[nodiscard]] static Gui::UiWidget* FindWidgetByNameRecursive(
			Gui::UiWidget* root,
			std::string_view widgetName
		);
		/// @brief lock spec を JSON へ書き出します。
		static void SerializeLockTarget(
			JsonWriter& writer,
			const LockTargetSpec& spec
		);
		/// @brief shot spec を JSON へ書き出します。
		static void SerializeShot(JsonWriter& writer, const ShotSpec& shot);
		/// @brief Branch相当の cubic-bezier 補間を返します。
		[[nodiscard]] static float EvaluateEase(float t);
		/// @brief エディター用にショットの軸デバッグ描画を行います。
		void DrawShotDebugAxes() const;

		uint64_t mCameraRootEntityGuid       = 0;
		uint64_t mCameraRotatorComponentGuid = 0;
		uint64_t mHudCanvasEntityGuid        = 0;
		uint64_t mCountAudioSourceGuid       = 0;
		uint64_t mStartAudioSourceGuid       = 0;
		std::string mSkipAction = "jump";

		std::string mCountdownDigitWidgetName = "OpeningCountdownDigit";
		std::string mCountdownStartWidgetName = "OpeningCountdownStart";
		std::string mFadeOverlayWidgetName    = "OpeningFadeOverlay";

		float mFadeOutSeconds        = 0.25f;
		float mFadeInSeconds         = 0.25f;
		float mCountdownDigitSeconds = 1.0f;
		float mCountdownStartSeconds = 0.8f;

		std::vector<ShotSpec> mShots = {
			{
				.startPos = Vec3(0.0f, 4.0f, 0.0f),
				.endPos = Vec3(0.0f, 4.0f, 40.0f),
				.startLook = Vec3(0.0f, 3.0f, 50.0f),
				.endLook = Vec3(0.0f, 3.0f, 50.0f),
				.durationSeconds = 3.0f,
			},
			{
				.startPos = Vec3(-52.0f, 8.0f, 118.0f),
				.endPos = Vec3(-52.0f, 8.0f, 118.0f),
				.startLook = Vec3(-28.0f, 1.0f, 140.0f),
				.endLook = Vec3(-100.0f, 1.0f, 119.0f),
				.durationSeconds = 4.0f,
			},
			{
				.startPos = Vec3(-113.794f, 75.0f, 92.0f),
				.endPos = Vec3(-113.794f, 75.0f, 5.0f),
				.startLook = Vec3(-113.794f, 0.0f, 92.5f),
				.endLook = Vec3(-113.794f, 0.0f, 5.5f),
				.durationSeconds = 3.0f,
			},
			{
				.startPos = Vec3(0.0f, 6.0f, 0.0f),
				.endPos = Vec3(0.0f, 3.626f, 0.0f),
				.startLook = Vec3(0.0f, 6.0f, 1.0f),
				.endLook = Vec3(0.0f, 3.626f, 1.0f),
				.durationSeconds = 0.5f,
			},
		};
		std::vector<LockTargetSpec> mLockTargets = {};

		PHASE mPhase = PHASE::GAMEPLAY;
		size_t mShotIndex = 0;
		float  mShotElapsedSeconds = 0.0f;
		float  mCountdownElapsedSeconds = 0.0f;
		float  mFadeAlpha = 0.0f;
		bool   mShotFadeActive = false;
		bool   mShotFadeSwapped = false;
		int    mLastCountdownCueStep = -1;
		bool   mSequenceActive = false;
		bool   mHasSavedGameplayPose = false;
		bool   mHasCountdownFixedPose = false;
		bool   mGameplayControlReleased = false;

		Vec3 mSavedGameplayCameraPosition = Vec3::zero;
		Vec2 mSavedGameplayLookAngles     = Vec2::zero;
		Vec3 mCountdownFixedPosition      = Vec3::zero;
		Vec2 mCountdownFixedLookAngles    = Vec2::zero;

		std::vector<ActiveLockState> mActiveLocks = {};

		Entity*                mCameraRootEntity      = nullptr;
		TransformComponent*    mCameraRootTransform   = nullptr;
		CameraRotatorComponent* mCameraRotator        = nullptr;
		UiCanvasComponent*     mHudCanvas             = nullptr;
		AudioSourceComponent*  mCountAudioSource      = nullptr;
		AudioSourceComponent*  mStartAudioSource      = nullptr;

		Gui::UiWidget*           mCountdownDigitWidget   = nullptr;
		Gui::UiTransformComponent* mCountdownDigitTransform = nullptr;
		Gui::UiTextureComponent* mCountdownDigitTexture  = nullptr;

		Gui::UiWidget*           mCountdownStartWidget   = nullptr;
		Gui::UiTransformComponent* mCountdownStartTransform = nullptr;
		Gui::UiTextureComponent* mCountdownStartTexture  = nullptr;

		Gui::UiWidget*           mFadeOverlayWidget      = nullptr;
		Gui::UiTextureComponent* mFadeOverlayTexture     = nullptr;
	};
}
