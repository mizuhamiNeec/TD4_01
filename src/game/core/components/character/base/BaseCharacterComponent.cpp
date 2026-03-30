#include "BaseCharacterComponent.h"

#include <algorithm>
#include <imgui.h>

#include "../state/GameMovementStateMachine.h"

#include "core/ComponentRegistry.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/math/Math.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"

#include "game/core/collision/kinematic/base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	constexpr float kDefaultMoveSpeedHu = 320.0f;

	BaseCharacterComponent::~BaseCharacterComponent() = default;

	void BaseCharacterComponent::AddMovementFrameInput(
		const MovementFrameInput& input
	) {
		mMoveFrameInput = input;
	}

	void BaseCharacterComponent::SimulateStep(
		TransformComponent*       transform,
		const MovementFrameInput& input,
		const float               stepSeconds
	) {
		if (!transform) {
			return;
		}

		// 移動速度をHUからMに変換
		const float moveSpeedM = Math::HtoM(kDefaultMoveSpeedHu);

		// 入力の視点方向
		const Vec3 right   = input.right.Normalized();
		const Vec3 up      = input.up.Normalized();
		const Vec3 forward = input.forward.Normalized();

		// カメラの基底ベクトルに入力軸を掛け合わせて、ワールド空間での移動方向を計算
		const Vec3 wishDir = right * input.moveAxis.x + forward * input.moveAxis
		                     .z +
		                     up * input.moveAxis.y;

		// 移動速度を計算
		mVelocity = wishDir.Normalized() * moveSpeedM;

		// デフォルトは移動するだけ
		Vec3 position = transform->Position();

		// 移動量を加算
		position += mVelocity * stepSeconds;

		// 位置を更新
		transform->SetPosition(position);
	}

	void BaseCharacterComponent::OnAttached() {
		BaseComponent::OnAttached();
	}

	void BaseCharacterComponent::PrePhysicsTick(const float deltaTime) {
		BaseComponent::PrePhysicsTick(deltaTime);
	}

	void BaseCharacterComponent::OnTick(const float deltaTime) {
		BaseComponent::OnTick(deltaTime);
	}

	void BaseCharacterComponent::PostPhysicsTick(const float deltaTime) {
		TransformComponent* transform = GetTransform();
		if (!transform) {
			Error(GetComponentName(), "TransformComponentが見つからないため、移動できません。");
			return;
		}

		// 移動は固定ステップでシミュレート
		mAccumulator               += deltaTime;
		const float maxAccumulated =
			mSimStepSec * static_cast<float>(mMaxSubSteps);
		mAccumulator = std::min(mAccumulator, maxAccumulated);

		uint32_t steps = 0;
		while (mAccumulator >= mSimStepSec && steps < mMaxSubSteps) {
			SimulateStep(transform, mMoveFrameInput, mSimStepSec);
			mAccumulator -= mSimStepSec;
			steps++;
		}
	}

	std::string_view BaseCharacterComponent::GetStableName() const {
		return "game.BaseCharacterComponent";
	}

	std::string_view BaseCharacterComponent::GetComponentName() const {
		return "BaseCharacterComponent";
	}

	void BaseCharacterComponent::Deserialize(const JsonReader& reader) {
		const JsonReader collisionEnabled = reader["collisionEnabled"];
		if (collisionEnabled.Valid()) {
			mCollisionEnabled = collisionEnabled.GetBool();
		}

		const JsonReader simStep = reader["simStepSec"];
		if (simStep.Valid()) {
			mSimStepSec = std::max(simStep.GetFloat(), 1.0f / 1000.0f);
		}

		const JsonReader maxSubSteps = reader["maxSubSteps"];
		if (maxSubSteps.Valid()) {
			mMaxSubSteps = std::max(1, maxSubSteps.GetInt());
		}

		const JsonReader boxHalfExtentsHu = reader["boxHalfExtentsHu"];
		if (boxHalfExtentsHu.Valid()) {
			mBoxHalfExtents = Math::HtoM(
				boxHalfExtentsHu.GetVec3(Math::MtoH(mBoxHalfExtents))
			);
		} else {
			const JsonReader boxHalfExtents = reader["boxHalfExtents"];
			if (boxHalfExtents.Valid()) {
				mBoxHalfExtents = boxHalfExtents.GetVec3(mBoxHalfExtents);
			}
		}
	}

	void BaseCharacterComponent::Serialize(JsonWriter& writer) const {
		writer.Key("collisionEnabled");
		writer.Write(mCollisionEnabled);

		writer.Key("simStepSec");
		writer.Write(mSimStepSec);

		writer.Key("maxSubSteps");
		writer.Write(static_cast<int>(mMaxSubSteps));

		const Vec3 boxHalfExtentsHu = Math::MtoH(mBoxHalfExtents);
		writer.WriteVec3("boxHalfExtentsHu", boxHalfExtentsHu);
	}

	uint32_t BaseCharacterComponent::GetIcon() const {
		return kIconDirectionsWalk;
	}

	TransformComponent* BaseCharacterComponent::GetTransform() const {
		if (Entity* owner = GetOwner()) {
			auto* transform = owner->GetComponent<TransformComponent>();
			if (transform) {
				return transform;
			}
			Error(GetComponentName(), "TransformComponentが見つかりませんでした。");
		} else {
			Error(
				GetComponentName(), "BaseCharacterComponentはエンティティにアタッチされていません。"
			);
		}
		return nullptr;
	}
#ifdef _DEBUG
	void BaseCharacterComponent::DrawInspectorImGui() {
		if (mStateMachine) {
			ImGui::Text(
				"CurrentState: %s",
				mStateMachine->GetCurrentState() ?
					mStateMachine->GetCurrentState()->GetStateName().data() :
					"Null"
			);
		}
		ImGui::Text("Grounded: %s", mGrounded ? "true" : "false");
		ImGui::Text("Vel: %f", Math::MtoH(mVelocity.Length()));

		Vec3 tmp = Math::MtoH(mVelocity);
		if (
			ImGuiWidgets::DragVec3(
				"Velocity", tmp, Vec3::zero, 1.0f, "%.2f"
			)
		) {
			mVelocity = Math::HtoM(tmp);
		}
		ImGui::Checkbox("Collision Enabled", &mCollisionEnabled);
		ImGui::DragFloat(
			"Sim Step Sec", &mSimStepSec, 0.0001f, 1.0f / 1000.0f,
			1.0f / 15.0f, "%.5f"
		);
		int maxSubSteps = static_cast<int>(mMaxSubSteps);
		if (ImGui::DragInt("Max Sub Steps", &maxSubSteps, 1.0f, 1, 16)) {
			mMaxSubSteps = static_cast<uint32_t>(std::max(1, maxSubSteps));
		}
		ImGui::DragFloat3(
			"Box Half Extents (m)", &mBoxHalfExtents.x, 0.01f, 0.01f,
			16.0f
		);
	}
#endif

	REGISTER_COMPONENT(BaseCharacterComponent);
}
