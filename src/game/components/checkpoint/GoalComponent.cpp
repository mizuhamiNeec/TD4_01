#include "GoalComponent.h"

#include <engine/Components/ColliderComponent/AABBCollider.h>
#include <engine/Debug/Debug.h>
#include <engine/Entity/Entity.h>
#include <engine/subsystem/console/Log.h>
#include <game/components/checkpoint/CheckpointManager.h>

#include "engine/OldConsole/Console.h"

void GoalComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	mCollider = owner.GetComponent<AABBCollider>();
}

void GoalComponent::Update(float) {
	if (!mCollider) {
		return;
	}

	CheckPlayerCollision();

	// デバッグ表示
	const Vec4          color     = mReached ? Vec4::blue : Vec4::magenta;
	const Unnamed::AABB worldAABB = mCollider->GetWorldAABB();
	const Vec3          center    = (worldAABB.min + worldAABB.max) * 0.5f;
	const Vec3          size      = worldAABB.Size();

	Debug::DrawBox(
		center,
		Quaternion::identity,
		size,
		color
	);
}

void GoalComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader(
		"GoalComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Reached: %s", mReached ? "Yes" : "No");

		if (ImGui::Button("Reset")) {
			Reset();
		}
	}
#endif
}

bool GoalComponent::IsReached() const {
	return mReached;
}

void GoalComponent::Reset() {
	mReached   = false;
	mWasInside = false;
}

void GoalComponent::CheckPlayerCollision() {
	// プレイヤーエンティティを取得
	Entity* player = CheckpointManager::GetPlayer();
	if (!player) {
		return;
	}

	// プレイヤーのAABBコライダーを取得
	auto* playerCollider = player->GetComponent<AABBCollider>();
	if (!playerCollider) {
		return;
	}

	// ワールド座標でのAABBを取得
	const Unnamed::AABB playerWorldAABB = playerCollider->GetWorldAABB();
	const Unnamed::AABB goalWorldAABB   = mCollider->GetWorldAABB();

	// AABB同士の衝突判定（標準的なAABB交差テスト）
	const bool isInside =
		playerWorldAABB.min.x <= goalWorldAABB.max.x && 
		playerWorldAABB.max.x >= goalWorldAABB.min.x &&
		playerWorldAABB.min.y <= goalWorldAABB.max.y && 
		playerWorldAABB.max.y >= goalWorldAABB.min.y &&
		playerWorldAABB.min.z <= goalWorldAABB.max.z && 
		playerWorldAABB.max.z >= goalWorldAABB.min.z;

	// 入った瞬間のみ判定
	if (isInside && !mWasInside && !mReached) {
		// 全チェックポイントを通過しているかチェック
		if (CheckpointManager::AreAllCheckpointsActivated()) {
			mReached = true;
			Console::Print("Goal reached!", Vec4(0.0f, 1.0f, 0.0f, 1.0f));
			// TODO: ゴール時の処理（タイム表示、リザルト画面など）
		} else {
			Console::Print(
				"You need to pass all checkpoints first!",
				Vec4(1.0f, 0.5f, 0.0f, 1.0f)
			);
		}
	}

	mWasInside = isInside;
}
