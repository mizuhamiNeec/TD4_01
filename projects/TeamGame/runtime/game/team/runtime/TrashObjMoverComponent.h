#pragma once
#include "engine/unnamed/framework/components/base/BaseComponent.h"
#include <core/io/json/JsonReader.h>
#include <core/io/json/JsonWriter.h>
#include <string>

namespace MyGame {

	/// @brief ゴミオブジェクトのコンポーネント
	/// ゴミの重量やタイプを管理するだけで、動きは物理エンジンに任せる
	class TrashObjMoverComponent : public Unnamed::BaseComponent {
	public:
		// -----------------------------------------------------------------------
		// ライフサイクル
		// -----------------------------------------------------------------------

		/// コンポーネントがアタッチされたときに呼び出される
		void OnAttached() override;

		/// 毎フレーム更新
		void OnTick(float deltaTime) override;

		/// コンポーネントがデタッチされたときに呼び出される
		void OnDetached() override;

		// -----------------------------------------------------------------------
		// ゴミオブジェクト設定
		// -----------------------------------------------------------------------

		/// @brief ゴミの重量を設定
		void SetMass(float mass);

		/// @brief ゴミの重量を取得
		[[nodiscard]] float GetMass() const;

		/// @brief ゴミのタイプを設定（例: "plastic", "metal", "organic"）
		void SetTrashType(const std::string& type);

		/// @brief ゴミのタイプを取得
		[[nodiscard]] const std::string& GetTrashType() const;

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
		/// ゴミの重量（物理エンジンが使用）
		float _mass = 1.0f;

		/// ゴミのタイプ（分類用）
		std::string _trashType = "generic";
	};

}
