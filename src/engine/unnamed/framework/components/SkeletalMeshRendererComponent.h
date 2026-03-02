#pragma once
#include <string>

#include "base/UBaseComponent.h"

#include "core/assets/AssetID.h"

namespace Unnamed {
	class AssetManager;
	class JsonReader;
	class JsonWriter;

	class SkeletalMeshRendererComponent final : public UBaseComponent {
	public:
		[[nodiscard]] std::string_view GetStableName() const override {
			return "engine.SkeletalMeshRenderer";
		}

		[[nodiscard]] std::string_view GetComponentName() const override {
			return "SkeletalMeshRenderer";
		}

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void SetMeshPath(const std::string& path);
		void SetMaterialInstancePath(const std::string& path);

		[[nodiscard]] const std::string& GetMeshPath() const noexcept;
		[[nodiscard]] const std::string&
		GetMaterialInstancePath() const noexcept;

		AssetID ResolveMeshAsset(AssetManager& assetManager);
		AssetID ResolveMaterialInstanceAsset(AssetManager& assetManager);

		[[nodiscard]] AssetID GetMeshAssetId() const noexcept;
		[[nodiscard]] AssetID GetMaterialInstanceAssetId() const noexcept;

	private:
		std::string mMeshPath;
		std::string mMaterialInstancePath;

		AssetID mMeshAssetId             = kInvalidAssetID;
		AssetID mMaterialInstanceAssetId = kInvalidAssetID;
	};
}
