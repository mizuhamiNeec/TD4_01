#include "engine/ResourceSystem/Material/MaterialManager.h"

#include <format>

#include "engine/OldConsole/Console.h"
#include "engine/ResourceSystem/Material/Material.h"
#include "engine/ResourceSystem/Pipeline/PipelineManager.h"

/// @brief マテリアル取得または作成（メッシュ名なし）
/// @param name マテリアル名
/// @param shader シェーダーポインタ
/// @return マテリアルポインタ
Material* MaterialManager::GetOrCreateMaterial(
	const std::string& name,
	Shader*            shader
) { return GetOrCreateMaterial(name, shader, ""); }

/// @brief マテリアル取得または作成（メッシュ名あり）
/// @param name マテリアル名
/// @param shader シェーダーポインタ
/// @param meshName メッシュ名
/// @return マテリアルポインタ
Material* MaterialManager::GetOrCreateMaterial(
	const std::string& name,
	Shader*            shader,
	const std::string& meshName
) {
	// キーを生成: メッシュ名がある場合は「メッシュ名_マテリアル名」、ない場合はマテリアル名のみ
	std::string key = GenerateMaterialKey(name, meshName);

	// 既に作成済みのマテリアルがあるか確認
	if (mMaterials.contains(key)) {
		Console::Print(
			std::format("MaterialManager: 既存マテリアル再利用 {} (キー: {})\n", name, key),
			kConTextColorCompleted,
			Channel::ResourceSystem
		);
		return mMaterials[key].get();
	}

	// なかったので作成
	auto material = std::make_unique<Material>(name, shader);

	// メッシュ名が指定されている場合は設定
	if (!meshName.empty()) { material->SetMeshName(meshName); }

	mMaterials[key] = std::move(material);

	Console::Print(
		std::format("MaterialManager: 新規マテリアル作成 {} (キー: {})\n", name, key),
		kConTextColorCompleted,
		Channel::ResourceSystem
	);

	return mMaterials[key].get();
}

/// @brief マテリアル取得（メッシュ名なし）
/// @param name マテリアル名
/// @return マテリアルポインタ
Material* MaterialManager::GetMaterial(const std::string& name) {
	return GetMaterial(name, "");
}

/// @brief マテリアル取得（メッシュ名あり）
/// @param name マテリアル名
/// @param meshName メッシュ名
/// @return マテリアルポインタ
Material* MaterialManager::GetMaterial(
	const std::string& name,
	const std::string& meshName
) {
	const std::string key = GenerateMaterialKey(name, meshName);
	const auto        it  = mMaterials.find(key);
	return it != mMaterials.end() ? it->second.get() : nullptr;
}

/// @brief マテリアルキー生成ヘルパー関数
/// @param materialName マテリアル名
/// @param meshName メッシュ名
/// @return 生成されたマテリアルキー
std::string MaterialManager::GenerateMaterialKey(
	const std::string& materialName, const std::string& meshName
) const {
	if (meshName.empty()) { return materialName; }
	return meshName + "_" + materialName;
}

/// @brief 初期化
void MaterialManager::Init() {
	Console::Print(
		"MaterialManager を初期化しています...\n",
		kConTextColorGray,
		Channel::ResourceSystem
	);
	mMaterials.clear();
}

/// @brief シャットダウン
void MaterialManager::Shutdown() {
	for (const auto& material : mMaterials) {
		Console::Print(
			"MatMgr: Releasing " + material.second->GetName() + "\n",
			Vec4::white,
			Channel::ResourceSystem
		);
		material.second->Shutdown();
	}

	PipelineManager::Shutdown();
	mMaterials.clear();
}
