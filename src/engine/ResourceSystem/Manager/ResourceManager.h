#pragma once
#include <memory>

#include "engine/renderer/SrvManager.h"
#include "engine/ResourceSystem/Animation/AnimationManager.h"
#include "engine/ResourceSystem/Material/MaterialManager.h"
#include "engine/ResourceSystem/Mesh/MeshManager.h"
#include "engine/ResourceSystem/Shader/ShaderManager.h"
#include "engine/TextureManager/TexManager.h"


class D3D12;

/// @brief リソースマネージャークラス
class ResourceManager {
public:
	ResourceManager(D3D12* d3d12);
	~ResourceManager() = default;

	void Init() const;
	void Shutdown();

	[[nodiscard]] SrvManager*       GetSrvManager() const;
	[[nodiscard]] TexManager*       GetTexManager() const;
	[[nodiscard]] ShaderManager*    GetShaderManager() const;
	[[nodiscard]] MaterialManager*  GetMaterialManager() const;
	[[nodiscard]] MeshManager*      GetMeshManager() const;
	[[nodiscard]] AnimationManager* GetAnimationManager() const;

private:
	D3D12* mD3d12;

	std::unique_ptr<SrvManager>       mSrvManager;
	std::unique_ptr<TexManager>       mTexManager;
	std::unique_ptr<ShaderManager>    mShaderManager;
	std::unique_ptr<MaterialManager>  mMaterialManager;
	std::unique_ptr<MeshManager>      mMeshManager;
	std::unique_ptr<AnimationManager> mAnimationManager;
};
