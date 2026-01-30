#pragma once
#include <d3d12.h>
#include <string>
#include <engine/renderer/IndexBuffer.h>
#include <engine/renderer/Structs.h>
#include <engine/Renderer/VertexBuffer.h>
#include <engine/ResourceSystem/Material/Material.h>

#include "engine/unnamed/uprimitive/UPrimitives.h"

/// @brief サブメッシュクラス
class SubMesh {
public:
	SubMesh(
		const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		std::string                                 name
	);
	~SubMesh();

	void SetVertexBuffer(const std::vector<Vertex>& vertices);
	void SetSkinnedVertexBuffer(const std::vector<SkinnedVertex>& vertices);
	void SetIndexBuffer(const std::vector<uint32_t>& indices);

	[[nodiscard]] Material* GetMaterial() const;
	void                    SetMaterial(Material* material);

	std::string& GetName();

	void Render(ID3D12GraphicsCommandList* commandList) const;

	void                           ReleaseResource();
	std::vector<Unnamed::Triangle> GetPolygons() const;

private:
	std::string                                  mName;
	Microsoft::WRL::ComPtr<ID3D12Device>         mDevice;
	std::unique_ptr<VertexBuffer<Vertex>>        mVertexBuffer;
	std::unique_ptr<VertexBuffer<SkinnedVertex>> mSkinnedVertexBuffer;
	std::unique_ptr<IndexBuffer>                 mIndexBuffer;
	Material*                                    mMaterial      = nullptr;
	bool                                         mIsSkinnedMesh = false;
};
