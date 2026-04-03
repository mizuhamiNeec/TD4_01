#include "BaseCharacterComponent.h"

#include <imgui.h>

#include "../state/GameMovementStateMachine.h"

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
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

	void BaseCharacterComponent::EnqueueDeterministicInput(
		const DeterministicInputPacket& packet
	) {
		mDeterministicInputPacket = packet;
	}

	void BaseCharacterComponent::EnqueueDeterministicInput(
		const uint64_t            tick,
		const float               stepSeconds,
		const MovementFrameInput& input
	) {
		DeterministicInputPacket packet;
		packet.tick        = tick;
		packet.stepSeconds = stepSeconds;
		packet.input       = input;
		EnqueueDeterministicInput(packet);
	}

	bool BaseCharacterComponent::TryDequeueDeterministicInput(
		DeterministicInputPacket& outPacket
	) {
		if (!mDeterministicInputPacket.has_value()) {
			return false;
		}
		outPacket = *mDeterministicInputPacket;
		mDeterministicInputPacket.reset();
		return true;
	}

	void BaseCharacterComponent::ClearDeterministicInputQueue() {
		mDeterministicInputPacket.reset();
	}

	size_t BaseCharacterComponent::GetDeterministicInputQueueSize() const {
		return mDeterministicInputPacket.has_value() ? 1ull : 0ull;
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
		ClearDeterministicInputQueue();
	}

	void BaseCharacterComponent::PrePhysicsTick(const float deltaTime) {
		BaseComponent::PrePhysicsTick(deltaTime);
	}

	void BaseCharacterComponent::OnTick(const float deltaTime) {
		BaseComponent::OnTick(deltaTime);
	}

	void BaseCharacterComponent::PostPhysicsTick(const float deltaTime) {
		BaseComponent::PostPhysicsTick(deltaTime);
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

		const Vec3 boxHalfExtentsHu = Math::MtoH(mBoxHalfExtents);
		writer.WriteVec3("boxHalfExtentsHu", boxHalfExtentsHu);
	}

	uint32_t BaseCharacterComponent::GetIcon() const {
		return kIconDirectionsWalk;
	}

	TransformComponent* BaseCharacterComponent::GetTransform() const {
		if (Entity* owner = GetOwner()) {
			if (auto* transform = owner->GetComponent<TransformComponent>()) {
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
		ImGui::DragFloat3(
			"Box Half Extents (m)", &mBoxHalfExtents.x, 0.01f, 0.01f,
			16.0f
		);
	}
#endif

	REGISTER_COMPONENT(BaseCharacterComponent);
}
