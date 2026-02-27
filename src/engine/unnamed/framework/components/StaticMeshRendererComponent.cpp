#include "StaticMeshRendererComponent.h"

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	namespace {
		std::string ReadStringOr(
			const JsonReader& reader, const char* key, const char* fallback
		) {
			const JsonReader value = reader[key];
			return value.Valid() ? value.GetString() : std::string(fallback);
		}
	}

	void StaticMeshRendererComponent::Deserialize(const JsonReader& reader) {
		std::string meshPath = ReadStringOr(reader, "meshPath", "");
		if (meshPath.empty()) { meshPath = ReadStringOr(reader, "mesh", ""); }

		std::string matPath = ReadStringOr(
			reader, "materialInstancePath", ""
		);
		if (matPath.empty()) { matPath = ReadStringOr(reader, "material", ""); }

		SetMeshPath(meshPath);
		SetMaterialInstancePath(matPath);
	}

	void StaticMeshRendererComponent::Serialize(JsonWriter& writer) const {
		writer.Key("meshPath");
		writer.Write(mMeshPath);
		writer.Key("materialInstancePath");
		writer.Write(mMaterialInstancePath);
	}

	void StaticMeshRendererComponent::SetMeshPath(const std::string& path) {
		const std::string normalized = path.empty() ?
			                               std::string() :
			                               StrUtil::NormalizePath(path);
		if (mMeshPath == normalized) { return; }

		mMeshPath    = normalized;
		mMeshAssetId = kInvalidAssetID;
	}

	void StaticMeshRendererComponent::SetMaterialInstancePath(
		const std::string& path
	) {
		const std::string normalized = path.empty() ?
			                               std::string() :
			                               StrUtil::NormalizePath(path);
		if (mMaterialInstancePath == normalized) { return; }

		mMaterialInstancePath    = normalized;
		mMaterialInstanceAssetId = kInvalidAssetID;
	}

	const std::string&
	StaticMeshRendererComponent::GetMeshPath() const noexcept {
		return mMeshPath;
	}

	const std::string&
	StaticMeshRendererComponent::GetMaterialInstancePath() const noexcept {
		return mMaterialInstancePath;
	}

	AssetID StaticMeshRendererComponent::ResolveMeshAsset(
		AssetManager& assetManager
	) {
		if (mMeshPath.empty()) { return kInvalidAssetID; }
		if (mMeshAssetId != kInvalidAssetID) { return mMeshAssetId; }

		mMeshAssetId = assetManager.LoadFromFile(mMeshPath, ASSET_TYPE::MESH);
		return mMeshAssetId;
	}

	AssetID StaticMeshRendererComponent::ResolveMaterialInstanceAsset(
		AssetManager& assetManager
	) {
		if (mMaterialInstancePath.empty()) { return kInvalidAssetID; }
		if (mMaterialInstanceAssetId != kInvalidAssetID) {
			return mMaterialInstanceAssetId;
		}

		mMaterialInstanceAssetId = assetManager.LoadFromFile(
			mMaterialInstancePath, ASSET_TYPE::MATERIAL_INSTANCE
		);
		return mMaterialInstanceAssetId;
	}

	AssetID StaticMeshRendererComponent::GetMeshAssetId() const noexcept {
		return mMeshAssetId;
	}

	AssetID
	StaticMeshRendererComponent::GetMaterialInstanceAssetId() const noexcept {
		return mMaterialInstanceAssetId;
	}

	REGISTER_COMPONENT(StaticMeshRendererComponent);
}
