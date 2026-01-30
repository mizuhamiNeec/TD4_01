#include "AABBCollider.h"

#include "engine/Debug/DebugDraw.h"
#include "engine/ImGui/ImGuiWidgets.h"

AABBCollider::AABBCollider(const Unnamed::AABB& aabb, const Vec3 offset)
	: mAABB(aabb),
	  mOffset(offset) {}

void AABBCollider::Update(float) {}

void AABBCollider::DrawInspectorImGui() {
#ifdef _DEBUG
	if (
		ImGui::CollapsingHeader("AABBCollider", ImGuiTreeNodeFlags_DefaultOpen)
	) {
		ImGui::SeparatorText("AABB");
		ImGuiWidgets::DragVec3("Min", mAABB.min, Vec3::zero, 0.1f, "%.2f");
		ImGuiWidgets::DragVec3("Max", mAABB.max, Vec3::zero, 0.1f, "%.2f");
		ImGui::SeparatorText("Offset");
		ImGuiWidgets::DragVec3("Offset", mOffset, Vec3::zero, 0.1f, "%.2f");
	}
#endif
}

void AABBCollider::CheckCollision(const ColliderComponent* other) const {
	(void)other;
}

Unnamed::AABB& AABBCollider::AABB() { return mAABB; }

Vec3& AABBCollider::Offset() { return mOffset; }

Unnamed::AABB AABBCollider::GetOffsetAABB() const {
	return Unnamed::AABB(
		mAABB.min + mOffset,
		mAABB.max + mOffset
	);
}

Unnamed::AABB AABBCollider::GetWorldAABB() const {
	if (!mOwner) { return mAABB; }

	const Vec3 worldPos = mOwner->GetTransform()->GetWorldPos();
	return Unnamed::AABB(
		worldPos + mOffset + mAABB.min,
		worldPos + mOffset + mAABB.max
	);
}
