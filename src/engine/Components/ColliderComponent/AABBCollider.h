#pragma once
#include <engine/Components/ColliderComponent/Base/ColliderComponent.h>

class AABBCollider : public ColliderComponent {
public:
	explicit AABBCollider(const Unnamed::AABB& aabb, Vec3 offset = Vec3::zero);

	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;
	void CheckCollision(const ColliderComponent* other) const override;

	Unnamed::AABB& AABB();
	Vec3&          Offset();

	[[nodiscard]] Unnamed::AABB GetOffsetAABB() const;

	/// @brief ワールド座標でのAABBを取得
	[[nodiscard]] Unnamed::AABB GetWorldAABB() const;

private:
	Unnamed::AABB mAABB;
	Vec3          mOffset = Vec3::zero;
};
