#pragma once
#include "../base/BaseComponent.h"

#include "core/assets/AssetID.h"
#include "core/math/Mat4.h"

namespace Unnamed {
	class StaticMeshColliderComponent final : public BaseComponent {
	public:
		void OnAttached() override;
		void OnDetached() override;
		void OnTick(float deltaTime) override;
		[[nodiscard]] TickGroup GetTickGroup() const override {
			return TickGroup::ColliderSync;
		}

		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

		[[nodiscard]] bool IsCollisionEnabled() const noexcept {
			return mEnabled;
		}

		[[nodiscard]] bool IsDynamicMesh() const noexcept {
			return mDynamic;
		}

	private:
		void RefreshPhysicsRegistration();
		void UnregisterPhysicsMesh();

		AssetID mRegisteredMeshAssetId = kInvalidAssetID;
		Mat4    mRegisteredWorld       = Mat4::zero;
		bool    mEnabled               = true;
		bool    mDynamic               = true;
		bool    mRegistered            = false;
		bool    mRegisteredAsDynamic   = false;
	};
}
