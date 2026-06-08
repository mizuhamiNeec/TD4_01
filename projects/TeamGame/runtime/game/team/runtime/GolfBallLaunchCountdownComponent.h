#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"

#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace Unnamed {
	class Entity;
}

namespace Unnamed::Gui {
	class UiDigitStripComponent;
	class UiWidget;
}

namespace MyGame {
	class GolfBallComponent;

	class GolfBallLaunchCountdownComponent : public Unnamed::BaseComponent {
	public:
		void OnAttached() override;
		void OnTick(float deltaTime) override;
		void OnDetached() override;

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const Unnamed::JsonReader& reader) override;
		void Serialize(Unnamed::JsonWriter& writer) const override;

		/// @brief 指定秒数で発射カウントダウンを開始する
		void StartCountdown(float seconds);

		/// @brief 現在の残り時間で発射カウントダウンを再開する
		void StartCountdown();

		/// @brief 発射カウントダウンを一時停止する
		void StopCountdown();

		/// @brief カウントダウンを初期状態へ戻す
		void ResetCountdown();

		/// @brief カウントダウン中かどうかを取得する
		[[nodiscard]] bool IsCountingDown() const;

		/// @brief カウントダウンが完了して発射済みかどうかを取得する
		[[nodiscard]] bool HasLaunched() const;

		/// @brief カウントダウンの残り時間を取得する
		[[nodiscard]] float GetRemainingTime() const;

		/// @brief カウントダウンの進行度を0.0～1.0で取得する
		[[nodiscard]] float GetProgress01() const;

	private:
		/// @brief カウントダウン対象のボールを取得する
		[[nodiscard]] GolfBallComponent* ResolveGolfBall() const;

		/// @brief GUID、タグ、名前の順でEntityを探す
		[[nodiscard]] Unnamed::Entity* ResolveEntity() const;

		/// @brief カウントダウンUIの数値を更新する
		void UpdateCountdownUi() const;

		/// @brief カウントダウン表示用のDigitStripを取得する
		[[nodiscard]] Unnamed::Gui::UiDigitStripComponent*
		ResolveCountdownDigitStrip() const;

		/// @brief UI階層から指定名のWidgetを再帰的に探す
		[[nodiscard]] Unnamed::Gui::UiWidget* FindWidgetByName(
			Unnamed::Gui::UiWidget* widget,
			std::string_view name
		) const;

		/// @brief trueの場合、起動後にカウントダウンして発射する
		bool _launchOnStart = true;

		/// @brief 発射までの待機時間
		float _countdownDuration = 15.0f;

		/// @brief 現在の残り時間
		float _countdownTimer = 15.0f;

		/// @brief 発射済みか
		bool _bHasLaunched = false;

		/// @brief 0の場合はタグまたは名前でボールを探す
		uint64_t _ballEntityGuid = 0;

		/// @brief GUID未指定時にボールを探すタグ
		std::string _ballTag = "Ball";

		/// @brief タグ検索に失敗した場合のボール検索名
		std::string _ballName = "Ball";

		/// @brief カウントダウン表示に使うUIドキュメント
		std::string _countdownUiAssetPath =
			"projects/TeamGame/content/ui/countdown.ui.json";

		/// @brief カウントダウン値を流し込むDigitStrip付きWidget名
		std::string _countdownDigitWidgetName = "number_UI";
	};
}
