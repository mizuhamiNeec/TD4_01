#pragma once
#include <engine/Components/ColliderComponent/Base/ColliderComponent.h>

#include "engine/uprimitive/UPrimitives.h"

class StaticMesh;
class StaticMeshRenderer;
struct AABB;

/// @brief 静的メッシュのコライダーコンポーネント
/// @details OBSOLETE!: 旧ゲームで衝突三角形取得のみに使っています。廃止予定。
class MeshColliderComponent : public ColliderComponent {
public:
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	void CheckCollision(const ColliderComponent* other) const override;
	std::vector<Unnamed::Triangle> GetTriangles();

	[[nodiscard]] StaticMesh* GetStaticMesh() const;

private:
	void                           BuildTriangleList();
	StaticMeshRenderer*            mMeshRenderer = nullptr;
	std::vector<Unnamed::Triangle> mTriangles;
};
