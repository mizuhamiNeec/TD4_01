#pragma once
#include <vector>

#include <game/gameframework/component/base/BaseComponent.h>

#include <runtime/core/math/Math.h>

namespace Unnamed {
	/// @class TransformComponent
	/// @brief エンティティに空間をもたせるコンポーネントです。
	class TransformComponent : public BaseComponent {
		struct RenderInstanceCache {
			float worldCol0[4];
			float worldCol1[4];
			float worldCol2[4];

			float normalCol0[4];
			float normalCol1[4];
			float normalCol2[4];
		};

	public:
		//---------------------------------------------------------------------
		// TransformComponent
		//---------------------------------------------------------------------
		[[nodiscard]] Vec3                Position() const noexcept;
		[[nodiscard]] Quaternion          Rotation() const noexcept;
		[[nodiscard]] Vec3                Scale() const noexcept;
		[[nodiscard]] TransformComponent* Parent() const;
		[[nodiscard]] const Mat4&         WorldMat() const noexcept;

		void SetPosition(const Vec3& newPosition);
		void SetRotation(const Quaternion& newRotation);
		void SetScale(const Vec3& newScale);
		void SetParent(TransformComponent* newParent);

		//---------------------------------------------------------------------
		// BaseComponent
		//---------------------------------------------------------------------
		void OnAttached() override;
		void OnDetached() override;

		void PrePhysicsTick(float deltaTime) override;
		void OnTick(float deltaTime) override;
		void PostPhysicsTick(float deltaTime) override;

		void OnPreRender() const override;
		void OnRender() const override;
		void OnPostRender() const override;

		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

		[[nodiscard]] std::string_view GetComponentName() const override;

		[[nodiscard]] const RenderInstanceCache& RenderCache() const noexcept;

		[[nodiscard]] uint64_t WorldRevision() const noexcept;

	private:
		void MarkDirty(); // 変化が合った場合のみ行列を更新する

		Vec3       mLocalPos   = Vec3::zero;
		Quaternion mLocalRot   = Quaternion::identity;
		Vec3       mLocalScale = Vec3::one;

		Mat4 mWorldMat = Mat4::identity;

		TransformComponent*              mParent = nullptr;
		std::vector<TransformComponent*> mChildren;

		bool mIsDirty = false; // 変更があったか?

		RenderInstanceCache mRenderCache = {};

		uint64_t mWorldRevision = 1;
	};
}
