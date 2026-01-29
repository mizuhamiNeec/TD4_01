#include <engine/ResourceSystem/Mesh/SubMesh.h>

#include "engine/OldConsole/Console.h"

/// @brief コンストラクタ
/// @param device D3D12デバイスへのCOMポインタ
/// @param name サブメッシュ名
SubMesh::SubMesh(
	const Microsoft::WRL::ComPtr<ID3D12Device>& device,
	std::string                                 name
) :
	mName(std::move(name)),
	mDevice(device) {}

/// @brief デストラクタ
SubMesh::~SubMesh() {}

/// @brief 頂点バッファを設定します
/// @param vertices 頂点データのベクター
void SubMesh::SetVertexBuffer(const std::vector<Vertex>& vertices) {
	size_t size   = vertices.size() * sizeof(Vertex);
	mVertexBuffer = std::make_unique<VertexBuffer<Vertex>>(
		mDevice, size, vertices.data()
	);
	mIsSkinnedMesh = false;
}

/// @brief スキニング頂点バッファを設定します
/// @param vertices スキニング頂点データのベクター
void SubMesh::SetSkinnedVertexBuffer(
	const std::vector<SkinnedVertex>& vertices
) {
	size_t size          = vertices.size() * sizeof(SkinnedVertex);
	mSkinnedVertexBuffer = std::make_unique<VertexBuffer<SkinnedVertex>>(
		mDevice, size, vertices.data()
	);
	mIsSkinnedMesh = true;
}

/// @brief インデックスバッファを設定します
/// @param indices インデックスデータのベクター
void SubMesh::SetIndexBuffer(const std::vector<uint32_t>& indices) {
	size_t size  = indices.size() * sizeof(uint32_t);
	mIndexBuffer = std::make_unique<IndexBuffer>(mDevice, size, indices.data());
}

/// @brief マテリアルを取得します
/// @return マテリアルへのポインタ
Material* SubMesh::GetMaterial() const { return mMaterial; }

/// @brief マテリアルを設定します
/// @param material マテリアルへのポインタ
void SubMesh::SetMaterial(Material* material) { mMaterial = material; }

/// @brief サブメッシュ名を取得します
/// @return サブメッシュ名への参照
std::string& SubMesh::GetName() { return mName; }

/// @brief サブメッシュ名を取得します
/// @return サブメッシュ名
void SubMesh::Render(ID3D12GraphicsCommandList* commandList) const {
	// 頂点バッファとインデックスバッファを設定
	if (mIsSkinnedMesh) {
		if (!mSkinnedVertexBuffer || !mIndexBuffer) {
			Console::Print(
				"スキニング頂点バッファまたはインデックスバッファが設定されていません",
				kConTextColorError, Channel::RenderSystem
			);
			return;
		}
		const D3D12_VERTEX_BUFFER_VIEW vbView = mSkinnedVertexBuffer->View();
		commandList->IASetVertexBuffers(0, 1, &vbView);
	} else {
		if (!mVertexBuffer || !mIndexBuffer) {
			Console::Print(
				"頂点バッファまたはインデックスバッファが設定されていません", kConTextColorError,
				Channel::RenderSystem
			);
			return;
		}
		const D3D12_VERTEX_BUFFER_VIEW vbView = mVertexBuffer->View();
		commandList->IASetVertexBuffers(0, 1, &vbView);
	}

	const D3D12_INDEX_BUFFER_VIEW ibView = mIndexBuffer->View();
	commandList->IASetIndexBuffer(&ibView);

	// トポロジ設定と描画
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawIndexedInstanced(
		static_cast<UINT>(mIndexBuffer->GetSize() / sizeof(uint32_t)), 1, 0, 0,
		0
	);
}

/// @brief リソースを解放します
void SubMesh::ReleaseResource() {
	if (mVertexBuffer) { mVertexBuffer.reset(); }
	if (mSkinnedVertexBuffer) { mSkinnedVertexBuffer.reset(); }
	if (mIndexBuffer) {
		//	indexBuffer_->Release();
		mIndexBuffer.reset();
	}
	mMaterial = nullptr;
}

/// @brief ポリゴン情報を取得します
std::vector<Unnamed::Triangle> SubMesh::GetPolygons() const {
	std::vector<Unnamed::Triangle> polygons;
	if (!mIndexBuffer) {
		Console::Print(
			"インデックスバッファが設定されていません", kConTextColorError,
			Channel::RenderSystem
		);
		return polygons;
	}

	const std::vector<uint32_t>& indices = mIndexBuffer->GetIndices();

	if (mIsSkinnedMesh) {
		if (!mSkinnedVertexBuffer) {
			Console::Print(
				"スキニング頂点バッファが設定されていません", kConTextColorError,
				Channel::RenderSystem
			);
			return polygons;
		}
		const std::vector<SkinnedVertex>& vertices = mSkinnedVertexBuffer->
			GetVertices();
		for (size_t i = 0; i < indices.size(); i += 3) {
			const SkinnedVertex& v0 = vertices[indices[i]];
			const SkinnedVertex& v1 = vertices[indices[i + 1]];
			const SkinnedVertex& v2 = vertices[indices[i + 2]];
			Vec3 v30 = {v0.position.x, v0.position.y, v0.position.z};
			Vec3 v31 = {v1.position.x, v1.position.y, v1.position.z};
			Vec3 v32 = {v2.position.x, v2.position.y, v2.position.z};

			polygons.emplace_back(v30, v31, v32);
		}
	} else {
		if (!mVertexBuffer) {
			Console::Print(
				"頂点バッファが設定されていません", kConTextColorError,
				Channel::RenderSystem
			);
			return polygons;
		}
		const std::vector<Vertex>& vertices = mVertexBuffer->GetVertices();
		for (size_t i = 0; i < indices.size(); i += 3) {
			const Vertex& v0  = vertices[indices[i]];
			const Vertex& v1  = vertices[indices[i + 1]];
			const Vertex& v2  = vertices[indices[i + 2]];
			Vec3          v30 = {v0.position.x, v0.position.y, v0.position.z};
			Vec3          v31 = {v1.position.x, v1.position.y, v1.position.z};
			Vec3          v32 = {v2.position.x, v2.position.y, v2.position.z};

			polygons.emplace_back(v30, v31, v32);
		}
	}
	return polygons;
}
