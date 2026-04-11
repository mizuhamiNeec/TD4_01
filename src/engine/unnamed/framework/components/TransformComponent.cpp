#include "TransformComponent.h"

#include "core/ComponentRegistry.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	Vec3 TransformComponent::Position() const noexcept {
	Vec3 TransformComponent::GetPosition() const noexcept {
		return mLocalPos;
	}

	Quaternion TransformComponent::GetRotation() const noexcept {
		return mLocalRot;
	}

	Vec3 TransformComponent::GetScale() const noexcept {
		return mLocalScale;
	}

	TransformComponent* TransformComponent::GetParent() const noexcept {
		return mParent;
	}

	const Mat4& TransformComponent::GetWorldMat() const noexcept {
		return mWorldMat;
	}

	const Mat4& TransformComponent::RenderWorldMat() const noexcept {
		mRenderWorldMat = BuildRenderLocalMatrix();
		if (mParent) {
			mRenderWorldMat = mRenderWorldMat * mParent->RenderWorldMat();
		}
		return mRenderWorldMat;
	}

	void TransformComponent::SetPosition(const Vec3 position) noexcept {
		mLocalPos = position;
		MarkDirty();
	}

	void TransformComponent::SetRotation(const Quaternion rotation) noexcept {
		mLocalRot = rotation;
		MarkDirty();
	}

	void TransformComponent::SetScale(const Vec3 scale) noexcept {
		mLocalScale = scale;
		MarkDirty();
	}

	void TransformComponent::SetParent(
		TransformComponent* parent, const bool preserveWorld
	) {
		if (mParent == parent) {
			Warning(
				GetComponentName(), "SetParent: 新しい親が現在の親と同じです。"
			);
			return;
		}

		if (mParent) {
			auto& children = mParent->mChildren;
			std::erase(children, this);
		}

		Vec3       preservedPosition = mLocalPos;
		Quaternion preservedRotation = mLocalRot;
		Vec3       preservedScale    = mLocalScale;
		if (preserveWorld) {
			Mat4 localWorld = mWorldMat;
			if (mIsDirty) {
				localWorld = Mat4::Affine(mLocalScale, mLocalRot, mLocalPos);
				if (mParent) {
					localWorld = localWorld * mParent->GetWorldMat();
				}
			}

			Mat4 newLocal = localWorld;
			if (parent) {
				newLocal = localWorld * parent->GetWorldMat().Inverse();
			}
			preservedPosition = newLocal.GetTranslate();
			preservedRotation = newLocal.ToQuaternion();
			preservedScale    = newLocal.GetScale();
		}

		// 新しい親を設定
		mParent = parent;
		if (mParent) {
			mParent->mChildren.emplace_back(this);
		}
		if (preserveWorld) {
			mLocalPos   = preservedPosition;
			mLocalRot   = preservedRotation;
			mLocalScale = preservedScale;
		}
		mPendingParentEntityGuid = 0;

		MarkDirty();
	}

	void TransformComponent::ResolveDeferredParent(const Scene& scene) {
		if (mPendingParentEntityGuid == 0) {
			return;
		}
		const Entity* parentEntity = scene.FindEntity(
			mPendingParentEntityGuid
		);
		if (!parentEntity) {
			mPendingParentEntityGuid = 0;
			return;
		}
		auto* parentTransform = const_cast<Entity*>(parentEntity)->GetComponent
		<
			TransformComponent>();
		SetParent(parentTransform, false);
		mPendingParentEntityGuid = 0;
	}

	Vec3 TransformComponent::Right() const noexcept {
		return mWorldMat.GetRight();
	}

	Vec3 TransformComponent::Up() const noexcept {
		return mWorldMat.GetUp();
	}

	Vec3 TransformComponent::Forward() const noexcept {
		return mWorldMat.GetForward();
	}

	void TransformComponent::OnDetached() {
		const auto children = mChildren;
		for (TransformComponent* child : children) {
			if (!child) {
				continue;
			}
			child->SetParent(nullptr);
		}
		if (mParent) {
			auto& siblings = mParent->mChildren;
			std::erase(siblings, this);
			mParent = nullptr;
		}
		mChildren.clear();
	}

	void TransformComponent::OnTick(float) {
		UpdateWorldRecursive();
	}

	void TransformComponent::OnEditorTick(const float deltaTime) {
		// エディタモードでも行列更新を行う
		OnTick(deltaTime);
	}

	std::string_view TransformComponent::GetStableName() const {
		return "engine.Transform";
	}

	std::string_view TransformComponent::GetComponentName() const {
		return "Transform";
	}

	uint32_t TransformComponent::GetIcon() const {
		return kIconDragPan;
	}

	void TransformComponent::Serialize(JsonWriter& writer) const {
		writer.Key("position");
		writer.BeginArray();
		writer.Write(mLocalPos.x);
		writer.Write(mLocalPos.y);
		writer.Write(mLocalPos.z);
		writer.EndArray();

		writer.Key("rotation");
		writer.BeginArray();
		writer.Write(mLocalRot.x);
		writer.Write(mLocalRot.y);
		writer.Write(mLocalRot.z);
		writer.Write(mLocalRot.w);
		writer.EndArray();

		writer.Key("scale");
		writer.BeginArray();
		writer.Write(mLocalScale.x);
		writer.Write(mLocalScale.y);
		writer.Write(mLocalScale.z);
		writer.EndArray();

		writer.Key("parentEntityGuid");
		writer.Write(
			mParent && mParent->GetOwner() ?
				mParent->GetOwner()->GetGuid() :
				0ull
		);
	}

	void TransformComponent::UpdateWorldRecursive() {
		if (mIsDirty) {
			// ローカル行列を計算
			const Mat4 localMat = Mat4::Affine(
				mLocalScale, mLocalRot, mLocalPos
			);

			// ワールド行列を計算
			if (mParent) {
				// 親がいる場合は親のワールド行列を掛ける
				mWorldMat = localMat * mParent->GetWorldMat();
			} else {
				// 親がいない場合はローカル行列がそのままワールド行列
				mWorldMat = localMat;
			}

			// フラグを外す
			mIsDirty = false;
		}

		for (TransformComponent* child : mChildren) {
			if (!child || !child->mIsDirty) {
				continue;
			}
			child->UpdateWorldRecursive();
		}
	}

	void TransformComponent::MarkDirty() {
		mIsDirty = true;

		// 子も再計算
		for (auto* child : mChildren) {
			child->MarkDirty();
		}
	}

#ifdef _DEBUG
	void TransformComponent::DrawInspectorImGui() {
		Vec3       localPos   = mLocalPos;
		Quaternion localRot   = mLocalRot;
		Vec3       localScale = mLocalScale;

		// Position 編集
		if (
			ImGuiWidgets::DragVec3(
				"Position", localPos, Vec3::zero, 0.1f, "%.3f"
			)
		) {
			SetPosition(localPos);
		}

		// Rotation 編集
		Vec3 eulerDegrees = localRot.ToEulerDegrees();
		if (
			ImGuiWidgets::DragVec3(
				"Rotation", eulerDegrees, Vec3::zero, 0.1f, "%.3f"
			)
		) {
			// 編集された Euler 角を Quaternion に変換
			localRot = Quaternion::EulerDegrees(eulerDegrees);
			SetRotation(localRot);
		}

		// Scale 編集
		if (
			ImGuiWidgets::DragVec3(
				"Scale", localScale, Vec3::one, 0.1f, "%.3f"
			)
		) {
			SetScale(localScale);
		}
	}
#endif

	void TransformComponent::Deserialize(const JsonReader& reader) {
		mLocalPos   = reader["position"].GetVec3(mLocalPos);
		mLocalRot   = reader["rotation"].GetQuaternion(mLocalRot);
		mLocalScale = reader["scale"].GetVec3(mLocalScale);

		mPendingParentEntityGuid =
			reader.ReadUint64("parentEntityGuid").value_or(0ull);

		MarkDirty();
	}

	// コンポーネント登録
	REGISTER_COMPONENT(TransformComponent);
}
