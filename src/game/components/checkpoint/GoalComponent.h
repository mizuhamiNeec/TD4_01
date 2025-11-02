#pragma once
#include <engine/Components/base/Component.h>

class AABBCollider;

/**
 * @brief ゴールコンポーネント
 * @details プレイヤーがゴールに到達したかを判定します。
 *          全てのチェックポイントを通過している必要があります。
 */
class GoalComponent : public Component {
public:
	GoalComponent() = default;

	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	/// @brief ゴールに到達したか
	[[nodiscard]] bool IsReached() const;

	/// @brief ゴール状態をリセット
	void Reset();

private:
	void CheckPlayerCollision();

	AABBCollider* mCollider  = nullptr;
	bool          mReached   = false;
	bool          mWasInside = false; // 前フレームで内部にいたか
};
