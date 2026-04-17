#pragma once

#include <string_view>

#include "engine/unnamed/framework/components/base/BaseComponent.h"

namespace Unnamed {
	/// @brief タイトル起動シーンからゲームシーンへ遷移するブートストラップコンポーネントです。
	class TitleBootstrapComponent final : public BaseComponent {
	public:
		/// @brief タイトル遷移処理を1回だけ実行します。
		/// @param deltaTime 前フレームからの経過時間（秒）
		void OnTick(float deltaTime) override;

		/// @brief コンポーネントの stable name を返します。
		[[nodiscard]] std::string_view GetStableName() const override;
		/// @brief コンポーネントの表示名を返します。
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t GetIcon() const override;

		/// @brief JSON から設定を読み込みます。
		void Deserialize(const JsonReader& reader) override;
		/// @brief 設定を JSON へ書き出します。
		void Serialize(JsonWriter& writer) const override;

	private:
		bool mTriggered = false;
	};
}

