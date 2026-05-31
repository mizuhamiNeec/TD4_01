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
		[[nodiscard]] MicComponent* ResolveMicComponent() const;
		[[nodiscard]] int32_t GetActiveBarCount(float percentage) const;
		[[nodiscard]] const std::string& GetActiveBarTexturePath(int32_t activeIndex) const;
		void EnsureMicStarted();

		uint64_t _micEntityGuid = 0;
		std::string _micEntityTag;
		std::array<std::string, 4> _levelTexturePaths = {
			"projects/TeamGame/content/textures/UI/voice/voiceUI_Level0.png",
			"projects/TeamGame/content/textures/UI/voice/voiceUI_Level1.png",
			"projects/TeamGame/content/textures/UI/voice/voiceUI_Level2.png",
			"projects/TeamGame/content/textures/UI/voice/voiceUI_Level3.png",
		};
		std::string _holeTexturePath =
			"projects/TeamGame/content/textures/UI/voice/voiceUI_Hole.png";
		Vec2 _position = Vec2(24.0f, 24.0f);
		Vec2 _barSize = Vec2(256.0f, 64.0f);
		Vec2 _holeSize = Vec2(256.0f, 128.0f);
		Vec2 _anchor = Vec2(0.0f, 0.0f);
		Vec4 _color = Vec4::one;
		int32_t _barCount = 8;
		float _barStepY = 52.0f;
		float _holeOffsetY = 384.0f;
		int32_t _sortKey = 1000;
		bool _autoStartMic = true;
		bool _micStartRequested = false;
	};
}
