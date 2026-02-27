#pragma once
#include <vector>

#include "base/UBaseComponent.h"

#include "core/math/Mat4.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec3.h"

namespace Unnamed {
	class TransformComponent : public UBaseComponent {
	public:
		//---------------------------------------------------------------------
		// TransformComponent
		//---------------------------------------------------------------------

		[[nodiscard]] Vec3                Position() const noexcept;
		[[nodiscard]] Quaternion          Rotation() const noexcept;
		[[nodiscard]] Vec3                Scale() const noexcept;
		[[nodiscard]] TransformComponent* Parent() const noexcept;
		[[nodiscard]] const Mat4&         WorldMat() const noexcept;

		void SetPosition(Vec3 position) noexcept;
		void SetRotation(Quaternion rotation) noexcept;
		void SetScale(Vec3 scale) noexcept;
		void SetParent(TransformComponent* parent);

		//---------------------------------------------------------------------
		// UBaseComponent
		//---------------------------------------------------------------------
		void OnTick(float deltaTime) override;

		[[nodiscard]] std::string_view GetStableName() const override {
			return "engine.Transform";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Transform";
		}

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		/// @brief 変更されたことをマークします。
		void MarkDirty();

		Vec3       mLocalPos   = Vec3::zero;
		Quaternion mLocalRot   = Quaternion::identity;
		Vec3       mLocalScale = Vec3::one;

		Mat4 mWorldMat = Mat4::identity;

		TransformComponent*              mParent = nullptr;
		std::vector<TransformComponent*> mChildren;

		bool mIsDirty = false;
	};
}
