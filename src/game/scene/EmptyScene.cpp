#include "EmptyScene.h"

#include "engine/Engine.h"
#include <engine/Components/ColliderComponent/MeshColliderComponent.h>
#include <engine/Components/MeshRenderer/SkeletalMeshRenderer.h>
#include <engine/Components/MeshRenderer/StaticMeshRenderer.h>
#include <engine/TextureManager/TexManager.h>

EmptyScene::~EmptyScene() = default;

void EmptyScene::Init() {
	mRenderer        = Unnamed::Engine::GetRenderer();
	mSrvManager      = Unnamed::Engine::GetSrvManager();
	mResourceManager = Unnamed::Engine::GetResourceManager();

	// {
	// 	TexManager::GetInstance()->LoadTexture("./content/core/textures/uvChecker.png", false);
	// }
}

void EmptyScene::Update(const float deltaTime) {
	// シーン内のすべてのエンティティを更新
	for (auto entity : mEntities) {
		if (entity->IsActive() && !entity->GetParent()) {
			entity->Update(deltaTime);
		}
	}
}

void EmptyScene::Render() {
	for (auto entity : mEntities) {
		if (entity->IsActive()) {
			entity->Render(mRenderer->GetCommandList());
		}
	}
}

void EmptyScene::Shutdown() {
}
