#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>

namespace MyGame {

/// @brief ゴルフボールの発射位置を示すコンポーネント
/// 
/// 責務：
/// - エンティティの位置をゴルフボールの発射開始位置として機能
/// - GolfBallComponent から参照される（マーカー的な役割）
/// 
/// 使用方法：
/// 1. このコンポーネントをエンティティにアタッチ
/// 2. エンティティの位置を発射地点に配置
/// 3. GolfBallComponent::SetStartPosEntity() でこのエンティティを指定
class GolfBallStartPosComponent : public Unnamed::BaseComponent {
public:
	// -----------------------------------------------------------------------
	// ライフサイクル
	// -----------------------------------------------------------------------

	/// コンポーネントがアタッチされたときに呼び出される
	void OnAttached() override;

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
};

} // namespace MyGame

