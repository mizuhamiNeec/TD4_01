#include "SkeletalMeshRendererComponent.h"

#include <algorithm>

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"
#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"

namespace Unnamed {
	namespace {
		std::string ReadStringOr(
			const JsonReader& reader, const char* key, const char* fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetString() : std::string(fallback);
		}
	}

	std::string_view SkeletalMeshRendererComponent::GetStableName() const {
		return "engine.SkeletalMeshRenderer";
	}

	std::string_view SkeletalMeshRendererComponent::GetComponentName() const {
		return "SkeletalMeshRenderer";
	}

	void SkeletalMeshRendererComponent::Deserialize(const JsonReader& reader) {
		std::string meshPath = ReadStringOr(reader, "meshPath", "");
		if (meshPath.empty()) {
			meshPath = ReadStringOr(reader, "mesh", "");
		}

		std::string matPath = ReadStringOr(
			reader, "materialInstancePath", ""
		);
		if (matPath.empty()) {
			matPath = ReadStringOr(reader, "material", "");
		}

		SetMeshPath(meshPath);
		SetMaterialInstancePath(matPath);
	}

	void SkeletalMeshRendererComponent::Serialize(JsonWriter& writer) const {
		writer.Key("meshPath");
		writer.Write(mMeshPath);
		writer.Key("materialInstancePath");
		writer.Write(mMaterialInstancePath);
	}

	uint32_t SkeletalMeshRendererComponent::GetIcon() const noexcept {
		return kIconAccessibility;
	}

#ifdef _DEBUG
	void SkeletalMeshRendererComponent::DrawInspectorImGui() {
		std::string meshPath = mMeshPath;
		if (
			ImGuiWidgets::AssetPathPicker(
				"SkeletalMeshPath",
				meshPath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::MESH)
			)
		) {
			SetMeshPath(meshPath);
		}

		std::string materialPath = mMaterialInstancePath;
		if (
			ImGuiWidgets::AssetPathPicker(
				"SkeletalMaterialPath",
				materialPath,
				ImGuiWidgets::AssetTypeToMask(ASSET_TYPE::MATERIAL_INSTANCE)
			)
		) {
			SetMaterialInstancePath(materialPath);
		}
	}
#endif

	void SkeletalMeshRendererComponent::SetMeshPath(const std::string& path) {
		const std::string normalized = path.empty() ?
			                               std::string() :
			                               StrUtil::NormalizePath(path);
		if (mMeshPath == normalized) {
			return;
		}

		mMeshPath    = normalized;
		mMeshAssetId = kInvalidAssetID;
	}

	void SkeletalMeshRendererComponent::SetMaterialInstancePath(
		const std::string& path
	) {
		const std::string normalized = path.empty() ?
			                               std::string() :
			                               StrUtil::NormalizePath(path);
		if (mMaterialInstancePath == normalized) {
			return;
		}

		mMaterialInstancePath    = normalized;
		mMaterialInstanceAssetId = kInvalidAssetID;
	}

	const std::string&
	SkeletalMeshRendererComponent::GetMeshPath() const noexcept {
		return mMeshPath;
	}

	const std::string&
	SkeletalMeshRendererComponent::GetMaterialInstancePath() const noexcept {
		return mMaterialInstancePath;
	}

	AssetID SkeletalMeshRendererComponent::ResolveMeshAsset(
		AssetManager& assetManager
	) {
		if (mMeshPath.empty()) {
			return kInvalidAssetID;
		}
		if (mMeshAssetId != kInvalidAssetID) {
			return mMeshAssetId;
		}

		mMeshAssetId = assetManager.LoadFromFile(mMeshPath, ASSET_TYPE::MESH);
		return mMeshAssetId;
	}

	AssetID SkeletalMeshRendererComponent::ResolveMaterialInstanceAsset(
		AssetManager& assetManager
	) {
		if (mMaterialInstancePath.empty()) {
			return kInvalidAssetID;
		}
		if (mMaterialInstanceAssetId != kInvalidAssetID) {
			return mMaterialInstanceAssetId;
		}

		mMaterialInstanceAssetId = assetManager.LoadFromFile(
			mMaterialInstancePath, ASSET_TYPE::MATERIAL_INSTANCE
		);
		return mMaterialInstanceAssetId;
	}

	AssetID SkeletalMeshRendererComponent::GetMeshAssetId() const noexcept {
		return mMeshAssetId;
	}

	AssetID
	SkeletalMeshRendererComponent::GetMaterialInstanceAssetId() const noexcept {
		return mMaterialInstanceAssetId;
	}

	REGISTER_COMPONENT(SkeletalMeshRendererComponent);
}
