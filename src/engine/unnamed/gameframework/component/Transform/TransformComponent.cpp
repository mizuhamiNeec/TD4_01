#include <pch.h>

#include <engine/unnamed/gameframework/component/Transform/TransformComponent.h>
#include <engine/unnamed/gameframework/entity/base/BaseEntity.h>

#include <core/unnamed/json/JsonReader.h>
#include <core/unnamed/json/JsonWriter.h>

#include "engine/Debug/Debug.h"

namespace Unnamed {
	/// @brief 位置を取得します。
	/// @return 位置
	Vec3 TransformComponent::Position() const noexcept {
		return mLocalPos;
	}

	/// @brief 回転を取得します。
	/// @return 回転
	Quaternion TransformComponent::Rotation() const noexcept {
		return mLocalRot;
	}

	/// @brief スケールを取得します。
	/// @return スケール
	Vec3 TransformComponent::Scale() const noexcept {
		return mLocalScale;
	}

	/// @brief 親の TransformComponent を取得します。
	/// @return 親の TransformComponent。親がいない場合は nullptr を返します。
	TransformComponent* TransformComponent::Parent() const {
		return mParent;
	}

	/// @brief ワールド行列を取得します。
	/// @return ワールド行列
	const Mat4& TransformComponent::WorldMat() const noexcept {
		return mWorldMat;
	}

	/// @brief 位置を設定します。
	/// @param newPosition 新しい位置
	void TransformComponent::SetPosition(const Vec3& newPosition) {
		mLocalPos = newPosition;
		MarkDirty();
	}

	/// @brief 回転を設定します。
	/// @param newRotation 新しい回転
	void TransformComponent::SetRotation(const Quaternion& newRotation) {
		mLocalRot = newRotation;
		MarkDirty();
	}

	/// @brief スケールを設定します。
	/// @param newScale 新しいスケール
	void TransformComponent::SetScale(const Vec3& newScale) {
		mLocalScale = newScale;
		MarkDirty();
	}

	/// @brief 親の TransformComponent を設定します。
	/// @param newParent 新しい親の TransformComponent。親を解除する場合は nullptr を指定します。
	void TransformComponent::SetParent(TransformComponent* newParent) {
		if (mParent == newParent) {
			Warning(
				GetComponentName(),
				"SetParent: 新しい親は現在の親と同じです。"
			);
			return;
		}

		// 前回の親から自分を削除
		if (mParent) {
			auto& children = mParent->mChildren;
			std::erase(children, this);
		}

		// 新しい親を設定
		mParent = newParent;
		if (mParent) {
			mParent->mChildren.emplace_back(this);
		}

		MarkDirty();
	}

	/// @brief コンポーネントがエンティティに取り付けられたときに呼び出されます。
	void TransformComponent::OnAttached() {
	}

	/// @brief コンポーネントがエンティティから取り外されたときに呼び出されます。
	void TransformComponent::OnDetached() {
	}

	/// @brief 物理演算の前に呼び出されます。
	void TransformComponent::PrePhysicsTick(float) {
	}

	/// @brief 毎フレーム呼び出されます。
	void TransformComponent::OnTick(float) {
		if (mIsDirty) {
			const Mat4 localMat =
				Mat4::Scale(mLocalScale) *
				Mat4::FromQuaternion(mLocalRot) *
				Mat4::Translate(mLocalPos);

			if (mParent) {
				mWorldMat = localMat * mParent->WorldMat();
			} else {
				mWorldMat = localMat;
			}

			const Mat4 world     = mWorldMat;
			const Mat4 worldInvT = world.Inverse().Transpose();

			auto FillCols3 = [](float outCol0[4], float       outCol1[4],
			                    float outCol2[4], const Mat4& m) {
				// col0
				outCol0[0] = m.m[0][0];
				outCol0[1] = m.m[1][0];
				outCol0[2] = m.m[2][0];
				outCol0[3] = m.m[3][0];
				// col1
				outCol1[0] = m.m[0][1];
				outCol1[1] = m.m[1][1];
				outCol1[2] = m.m[2][1];
				outCol1[3] = m.m[3][1];
				// col2
				outCol2[0] = m.m[0][2];
				outCol2[1] = m.m[1][2];
				outCol2[2] = m.m[2][2];
				outCol2[3] = m.m[3][2];
			};

			FillCols3(
				mRenderCache.worldCol0, mRenderCache.worldCol1,
				mRenderCache.worldCol2, world
			);
			FillCols3(
				mRenderCache.normalCol0, mRenderCache.normalCol1,
				mRenderCache.normalCol2, worldInvT
			);

			++mWorldRevision;

			mIsDirty = false;
		}
	}

	/// @brief 物理演算の後に呼び出されます。
	void TransformComponent::PostPhysicsTick(float) {
	}

	/// @brief レンダリングの前に呼び出されます。
	void TransformComponent::OnPreRender() const {
	}

	/// @brief レンダリング時に呼び出されます。
	void TransformComponent::OnRender() const {
	}

	/// @brief レンダリングの後に呼び出されます。
	void TransformComponent::OnPostRender() const {
	}

	/// @brief コンポーネントの状態をシリアライズします。
	/// @param writer JSON ライター
	void TransformComponent::Serialize(JsonWriter& writer) const {
		if (mParent) {
			writer.Key("parentId");
			writer.Write(mParent->GetOwner()->GetId());
		}

		writer.Key("guid");
		writer.Write(GetId());

		writer.Key("pos");
		writer.Write(
			std::vector{mLocalPos.x, mLocalPos.y, mLocalPos.z}
		);

		writer.Key("rot");
		writer.Write(
			std::vector{mLocalRot.x, mLocalRot.y, mLocalRot.z, mLocalRot.w}
		);

		writer.Key("scale");
		writer.Write(
			std::vector{mLocalScale.x, mLocalScale.y, mLocalScale.z}
		);

		writer.Key("type");
		writer.Write(GetComponentName());
	}

	/// @brief コンポーネントの状態をデシリアライズします。
	/// @param reader JSON リーダー
	void TransformComponent::Deserialize(const JsonReader& reader) {
		if (
			auto p = reader.Read<std::vector<float>>("pos");
			p && p->size() == 3
		) {
			mLocalPos = {(*p)[0], (*p)[1], (*p)[2]};
		}

		if (
			auto r = reader.Read<std::vector<float>>("rot");
			r && r->size() == 4
		) {
			mLocalRot = {(*r)[0], (*r)[1], (*r)[2], (*r)[3]};
		}

		if (
			auto s = reader.Read<std::vector<float>>("scale");
			s && s->size() == 3
		) {
			mLocalScale = {(*s)[0], (*s)[1], (*s)[2]};
		}

		// if (
		// 	auto parentId = reader.Read<uint64_t>("parentId");
		// 	parentId && mOwner
		// ) {
		// 	auto* parentEntity = mOwner->GetWorld()->GetEntityById(*parentId);
		// 	if (parentEntity) {
		// 		auto* parentTransform =
		// 			parentEntity->GetComponent<TransformComponent>();
		// 		if (parentTransform) {
		// 			SetParent(parentTransform);
		// 		} else {
		// 			Warning(
		// 				GetComponentName(),
		// 				"Deserialize: 親の TransformComponent が見つかりません。"
		// 			);
		// 		}
		// 	} else {
		// 		Warning(
		// 			GetComponentName(),
		// 			"Deserialize: 親のエンティティが見つかりません。"
		// 		);
		// 	}
		// }
	}

	/// @brief コンポーネントの名前を取得します。
	/// @return コンポーネントの名前
	std::string_view TransformComponent::GetComponentName() const {
		return "Transform";
	}

	/// @brief レンダー用のインスタンスキャッシュを取得します。
	/// @return レンダー用のインスタンスキャッシュ
	const TransformComponent::RenderInstanceCache&
	TransformComponent::RenderCache() const noexcept {
		return mRenderCache;
	}

	/// @brief ワールドのリビジョン番号を取得します。
	/// @return ワールドのリビジョン番号
	uint64_t TransformComponent::WorldRevision() const noexcept {
		return mWorldRevision;
	}

	/// @brief 自身と子孫の TransformComponent をすべてDirty状態にします。
	void TransformComponent::MarkDirty() {
		mIsDirty = true;
		for (auto* child : mChildren) {
			child->MarkDirty();
		}
	}
}
