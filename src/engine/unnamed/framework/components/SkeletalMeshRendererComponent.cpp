#include "SkeletalMeshRendererComponent.h"

#include <algorithm>
#include <array>

#include <imgui.h>

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

	void SkeletalMeshRendererComponent::Deserialize(const JsonReader& reader) {
		std::string meshPath = ReadStringOr(reader, "meshPath", "");
		if (meshPath.empty()) { meshPath = ReadStringOr(reader, "mesh", ""); }

		std::string matPath = ReadStringOr(
			reader, "materialInstancePath", ""
		);
		if (matPath.empty()) { matPath = ReadStringOr(reader, "material", ""); }

		SetMeshPath(meshPath);
		SetMaterialInstancePath(matPath);
	}

	void SkeletalMeshRendererComponent::Serialize(JsonWriter& writer) const {
		writer.Key("meshPath");
		writer.Write(mMeshPath);
		writer.Key("materialInstancePath");
		writer.Write(mMaterialInstancePath);
	}

#ifdef _DEBUG
	void SkeletalMeshRendererComponent::DrawInspectorImGui() {
		std::array<char, 512> meshPath = {};
		std::array<char, 512> matPath  = {};

		memcpy(
			meshPath.data(),
			mMeshPath.c_str(),
			std::min(mMeshPath.size(), meshPath.size() - 1)
		);
		memcpy(
			matPath.data(),
			mMaterialInstancePath.c_str(),
			std::min(mMaterialInstancePath.size(), matPath.size() - 1)
		);

		if (
			ImGui::InputText(
				"SkeletalMeshPath", meshPath.data(), meshPath.size()
			)
		) { SetMeshPath(meshPath.data()); }
		if (
			ImGui::InputText(
				"SkeletalMaterialPath", matPath.data(), matPath.size()
			)
		) { SetMaterialInstancePath(matPath.data()); }
	}
#endif

	void SkeletalMeshRendererComponent::SetMeshPath(const std::string& path) {
		const std::string normalized = path.empty() ?
			                               std::string() :
			                               StrUtil::NormalizePath(path);
		if (mMeshPath == normalized) { return; }

		mMeshPath    = normalized;
		mMeshAssetId = kInvalidAssetID;
	}

	void SkeletalMeshRendererComponent::SetMaterialInstancePath(
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
		if (mMeshPath.empty()) { return kInvalidAssetID; }
		if (mMeshAssetId != kInvalidAssetID) { return mMeshAssetId; }

		mMeshAssetId = assetManager.LoadFromFile(mMeshPath, ASSET_TYPE::MESH);
		return mMeshAssetId;
	}

	AssetID SkeletalMeshRendererComponent::ResolveMaterialInstanceAsset(
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

	AssetID SkeletalMeshRendererComponent::GetMeshAssetId() const noexcept {
		return mMeshAssetId;
	}

	AssetID
	SkeletalMeshRendererComponent::GetMaterialInstanceAssetId() const noexcept {
		return mMaterialInstanceAssetId;
	}

	REGISTER_COMPONENT(SkeletalMeshRendererComponent);
}
