#include "engine/ResourceSystem/Manager/ResourceManager.h"

#include "engine/OldConsole/Console.h"
#include "engine/renderer/D3D12.h"
#include "engine/renderer/SrvManager.h"
#include "engine/ResourceSystem/RootSignature/RootSignatureManager2.h"
#include "engine/TextureManager/TexManager.h"

/// @brief コンストラクタ
/// @param d3d12 D3D12クラスへのポインタ
ResourceManager::ResourceManager(D3D12* d3d12) :
	d3d12_(d3d12),
	mSrvManager(nullptr),
	mTexManager(nullptr),
	mShaderManager(nullptr),
	mMaterialManager(nullptr),
	mMeshManager(nullptr) {
	mSrvManager       = std::make_unique<SrvManager>();
	mTexManager       = std::make_unique<TexManager>();
	mShaderManager    = std::make_unique<ShaderManager>();
	mMaterialManager  = std::make_unique<MaterialManager>();
	mMeshManager      = std::make_unique<MeshManager>();
	mAnimationManager = std::make_unique<AnimationManager>();
}

/// @brief 初期化
void ResourceManager::Init() const {
	Console::Print(
		"ResourceManager を初期化しています...\n", kConTextColorWait,
		Channel::ResourceSystem
	);
	// マネージャーを初期化
	mSrvManager->Init(d3d12_);
	mTexManager->Init(d3d12_, mSrvManager.get());
	RootSignatureManager2::Init(d3d12_->GetDevice());
	mShaderManager->Init();
	mMaterialManager->Init();
	mMeshManager->Init(
		d3d12_->GetDevice(),
		mShaderManager.get(), mMaterialManager.get()
	);

	mAnimationManager->Init();

	Console::Print(
		"ResourceManager の初期化が完了しました\n", kConTextColorCompleted,
		Channel::ResourceSystem
	);
}

/// @brief 終了処理
void ResourceManager::Shutdown() {
	Console::Print(
		"ResourceManager を終了しています...\n", kConTextColorWait,
		Channel::ResourceSystem
	);

	// 全てのリソースマネージャーを終了

	if (mAnimationManager) {
		mAnimationManager->Shutdown();
		mAnimationManager.reset();
	}

	if (mMeshManager) {
		mMeshManager->Shutdown();
		mMeshManager.reset();
	}

	if (mMaterialManager) {
		mMaterialManager->Shutdown();
		mMaterialManager.reset();
	}

	if (mShaderManager) {
		mShaderManager->Shutdown();
		mShaderManager.reset();
	}

	if (mTexManager) {
		mTexManager.reset();
	}

	RootSignatureManager2::Shutdown();
}

/// @brief SrvManagerを取得
/// @return SrvManagerへのポインタ
SrvManager* ResourceManager::GetSrvManager() const { return mSrvManager.get(); }

/// @brief TexManagerを取得
/// @return TexManagerへのポインタ
TexManager* ResourceManager::GetTexManager() const {
	return mTexManager.get();
}

/// @brief MeshManagerを取得
/// @return MeshManagerへのポインタ
MeshManager* ResourceManager::GetMeshManager() const {
	return mMeshManager.get();
}

/// @brief AnimationManagerを取得
/// @return AnimationManagerへのポインタ
AnimationManager* ResourceManager::GetAnimationManager() const {
	return mAnimationManager.get();
}

/// @brief MaterialManagerを取得
/// @return MaterialManagerへのポインタ
MaterialManager* ResourceManager::GetMaterialManager() const {
	return mMaterialManager.get();
}

/// @brief ShaderManagerを取得
/// @return ShaderManagerへのポインタ
ShaderManager* ResourceManager::GetShaderManager() const {
	return mShaderManager.get();
}
