#pragma once
#include <vector>

#include "base/BaseComponent.h"

#include "core/math/Mat4.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec3.h"

namespace Unnamed {
	class Scene;

	class TransformComponent : public BaseComponent {
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
		void SetParent(TransformComponent* parent, bool preserveWorld = true);
		void ResolveDeferredParent(const Scene& scene);

		[[nodiscard]] Vec3 Right() const noexcept;
		[[nodiscard]] Vec3 Up() const noexcept;
		[[nodiscard]] Vec3 Forward() const noexcept;

		//---------------------------------------------------------------------
		// BaseComponent
		//---------------------------------------------------------------------
		void OnDetached() override;
		void OnTick(float deltaTime) override;
		void OnEditorTick(float deltaTime) override;

		[[nodiscard]] std::string_view GetStableName() const override {
			return "engine.Transform";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "Transform";
		}

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

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
		uint64_t                         mPendingParentEntityGuid = 0;

		bool mIsDirty = false;
	};
}
