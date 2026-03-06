#pragma once
#include <memory>

#include "core/math/Quaternion.h"
#include "core/math/Vec3.h"

#include "engine/unnamed/framework/components/base/UBaseComponent.h"

namespace Unnamed {
	struct MoveData {
		Quaternion viewAngles = Quaternion::identity; // プレイヤー視点の回転
		Vec3       velocity   = Vec3::zero;           // プレイヤーの現在の速度

		float forwardMove  = 0.0f; // 前進方向への入力
		float sideMove     = 0.0f; // 横方向への入力
		float upMove       = 0.0f; // 上方向への入力
		float maxMoveSpeed = 0.0f; // プレイヤーが地面で移動できる最大速度
	};

	/// @brief 衝突付きでの移動を処理するコンポーネントです。
	class BaseCharacter : public UBaseComponent {
	public:
		// ---- BaseCharacter -------------------------------------------------
		BaseCharacter();
		~BaseCharacter() override;

		void OnAttached() override;
		void OnDetached() override;
		void PrePhysicsTick(float deltaTime) override;
		void OnTick(float deltaTime) override;
		void PostPhysicsTick(float deltaTime) override;
		void DrawInspectorImGui() override;

		// ---- UBaseComponent ------------------------------------------------
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;
		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		std::unique_ptr<MoveData> mMoveData = nullptr;
	};
}
