#pragma once

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	/// @brief TeamGame の最小サンプル用コンポーネントです。
	/// @details 起動時にログを出し、ゲーム固有コンポーネント追加位置の基準として使います。
	class TeamSampleComponent final : public BaseComponent {
	public:
		/// @brief Entity に取り付けられた直後に呼び出されます。
		void OnAttached() override;

		/// @brief シーン保存時に参照される安定名を返します。
		[[nodiscard]] std::string_view GetStableName() const override {
			return "team.TeamSample";
		}

		/// @brief エディタ表示用のコンポーネント名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Team Sample";
		}

		/// @brief JSON からコンポーネント状態を復元します。
		void Deserialize(const JsonReader& reader) override;
		/// @brief JSON へコンポーネント状態を書き出します。
		void Serialize(JsonWriter& writer) const override;
	};
}
