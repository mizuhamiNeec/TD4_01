#include <engine/ResourceSystem/Mesh/StaticMesh.h>

#include <engine/unnamed/uprimitive/UPrimitives.h>

/// @brief コンストラクタ
/// @param name メッシュ名
void StaticMesh::AddSubMesh(std::unique_ptr<SubMesh> subMesh) {
	mSubMeshes.emplace_back(std::move(subMesh));
}

/// @brief サブメッシュの取得
/// @return サブメッシュのベクターへの参照
const std::vector<std::unique_ptr<SubMesh>>& StaticMesh::GetSubMeshes() const {
	return mSubMeshes;
}

/// @brief メッシュ名の取得
/// @return メッシュ名
std::string StaticMesh::GetName() const { return mName; }

/// @brief ポリゴン情報の取得
/// @return ポリゴン情報のベクター
std::vector<Unnamed::Triangle> StaticMesh::GetPolygons() const {
	std::vector<Unnamed::Triangle> polygons;
	for (const auto& subMesh : mSubMeshes) {
		auto subMeshPolygons = subMesh->GetPolygons();
		polygons.insert(
			polygons.end(), subMeshPolygons.begin(),
			subMeshPolygons.end()
		);
	}
	return polygons;
}

/// @brief レンダリング
/// @param commandList コマンドリスト
void StaticMesh::Render(ID3D12GraphicsCommandList* commandList) const {
	for (const auto& subMesh : mSubMeshes) { subMesh->Render(commandList); }
}

/// @brief リソースの解放
void StaticMesh::ReleaseResource() {
	for (const auto& subMesh : mSubMeshes) { subMesh->ReleaseResource(); }
	mSubMeshes.clear();
}
