#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <./core/math/Vec2.h>
#include <./core/math/Vec3.h>
#include <./core/math/Vec4.h>

#include <array>
#include <cstdint>
#include <string>

namespace MyGame {
	class MicComponent;

	class VoiceUiComponent : public Unnamed::BaseComponent {
	public:
		// -----------------------------------------------------------------------
		// ライフサイクル
		// -----------------------------------------------------------------------

		/// コンポーネントがアタッチされたときに呼び出される
		void OnAttached() override;

		/// 毎フレーム更新
		void OnTick(float deltaTime) override;

		/// 描画フレーム更新
		void OnRenderTick(float renderDeltaTime, float interpolationAlpha) override;

		/// コンポーネントがデタッチされたときに呼び出される
		void OnDetached() override;

		// -----------------------------------------------------------------------
		// BaseComponent override
		// -----------------------------------------------------------------------

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		/// コンポーネントの値を読み込む際に使用されます
		void Deserialize(const Unnamed::JsonReader& reader) override;

		/// コンポーネントの値を書き込む際に使用されます
		void Serialize(Unnamed::JsonWriter& writer) const override;

	private:
		/// 使用する MicComponent を取得する
		[[nodiscard]] MicComponent* ResolveMicComponent() const;

		/// 音量パーセントから点灯するバーの本数を計算する
		[[nodiscard]] int32_t GetActiveBarCount(float percentage) const;

		/// 下から数えた点灯バー番号に応じて色付きバー画像を選ぶ
		[[nodiscard]] const std::string& GetActiveBarTexturePath(int32_t activeIndex) const;

		/// UI表示に必要なマイク入力を開始する
		void EnsureMicStarted();

		/// 参照する MicComponent を持つ Entity GUID。0 の場合は自動検索
		uint64_t _micEntityGuid = 0;

		/// GUID 未指定時に参照する MicComponent を探すための Entity タグ
		std::string _micEntityTag;

		/// 0=灰色、1=緑、2=オレンジ、3=赤 のバー画像
		std::array<std::string, 4> _levelTexturePaths = {
			"projects/TeamGame/content/textures/UI/voice/voiceUI_Level0.png",
			"projects/TeamGame/content/textures/UI/voice/voiceUI_Level1.png",
			"projects/TeamGame/content/textures/UI/voice/voiceUI_Level2.png",
			"projects/TeamGame/content/textures/UI/voice/voiceUI_Level3.png",
		};

		/// メーター下部に表示する穴の画像
		std::string _holeTexturePath =
			"projects/TeamGame/content/textures/UI/voice/voiceUI_Hole.png";

		/// UI 全体の左上位置
		Vec2 _position = Vec2(24.0f, 24.0f);

		/// バー1本あたりの描画サイズ
		Vec2 _barSize = Vec2(256.0f, 64.0f);

		/// 穴画像の描画サイズ
		Vec2 _holeSize = Vec2(256.0f, 128.0f);

		/// 各スプライトのアンカー。標準では左上基準
		Vec2 _anchor = Vec2(0.0f, 0.0f);
		Vec4 _color = Vec4::one;

		/// 音量メーターのバー本数
		int32_t _barCount = 8;

		/// バー同士の縦方向の間隔
		float _barStepY = 52.0f;

		/// UI 左上から穴画像までの縦方向オフセット
		float _holeOffsetY = 384.0f;
		int32_t _sortKey = 1000;

		/// true の場合、UI側から MicComponent::StartMic を呼ぶ
		bool _autoStartMic = true;

		/// StartMic の多重呼び出しを避けるためのフラグ
		bool _micStartRequested = false;
	};
}
