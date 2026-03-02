#include "UGameWorld.h"

#include "core/string/StrUtil.h"

#include "engine/scene/USceneSerializer.h"
#include "engine/render/frame/RenderFrameContext.h"
#include "engine/render/frame/RenderFrameInputs.h"

#include "game/parkour/ParkourRuntime.h"

namespace Unnamed {
	UGameWorld::~UGameWorld() = default;

	void UGameWorld::Initialize() {
		UWorld::Initialize();
		if (!mRuntime) { mRuntime = std::make_unique<ParkourRuntime>(*this); }
		mRuntime->Initialize();
		LoadSceneFromFile(ResolveScenePath(mActiveSceneId));
	}

	void UGameWorld::Shutdown() {
		if (mRuntime) { mRuntime->OnSceneUnloaded(); }
		UWorld::Shutdown();
	}

	void UGameWorld::Tick(const float deltaTime) {
		AttachRuntimeToCurrentScene();
		if (mRuntime) { mRuntime->PreWorldTick(deltaTime); }
		UWorld::Tick(deltaTime);
		if (mRuntime) { mRuntime->PostWorldTick(deltaTime); }
		ApplyPendingSceneChange();
	}

	bool UGameWorld::LoadSceneFromFile(const char* path) {
		if (!path || std::string_view(path).empty()) { return false; }

		auto       newScene = std::make_unique<UScene>();
		const bool ok       = USceneSerializer::LoadFromFile(
			*newScene, path, mGuidGenerator
		);
		if (!ok) { return false; }

		UnloadScene();
		UWorld::SetScene(std::move(newScene));
		mLoadedScenePath       = StrUtil::NormalizePath(path);
		mSceneRuntimeAttached  = false;
		AttachRuntimeToCurrentScene();
		return true;
	}

	void UGameWorld::UnloadScene() {
		if (!mScene) { return; }
		if (mRuntime) { mRuntime->OnSceneUnloaded(); }
		mSceneRuntimeAttached = false;
		UWorld::UnloadScene();
	}

	void UGameWorld::FillRenderFrameInputs(
		Render::RenderFrameInputs&  inputs,
		Render::RenderFrameContext& frameContext,
		AssetManager&               assetManager
	) {
		if (mRuntime) { mRuntime->PrepareRender(inputs.sceneRenderRequest); }
		UWorld::FillRenderFrameInputs(inputs, frameContext, assetManager);
		if (mRuntime) { mRuntime->BuildRenderContributions(inputs, assetManager); }
	}

	void UGameWorld::SetScene(std::unique_ptr<UScene> scene) {
		if (mScene) { UnloadScene(); }
		UWorld::SetScene(std::move(scene));
		mSceneRuntimeAttached = false;
		AttachRuntimeToCurrentScene();
	}

	void UGameWorld::RequestSceneChange(const GameSceneId target) {
		mPendingSceneId         = target;
		mHasPendingSceneChange  = true;
	}

	void UGameWorld::ApplyPendingSceneChange() {
		if (!mHasPendingSceneChange) { return; }
		mHasPendingSceneChange = false;
		mActiveSceneId         = mPendingSceneId;
		LoadSceneFromFile(ResolveScenePath(mActiveSceneId));
	}

	void UGameWorld::AttachRuntimeToCurrentScene() {
		if (mSceneRuntimeAttached || !mScene || !mRuntime) { return; }
		mRuntime->OnSceneLoaded(*mScene);
		mSceneRuntimeAttached = true;
	}

	const char* UGameWorld::ResolveScenePath(const GameSceneId sceneId) const {
		switch (sceneId) {
			case GameSceneId::Title: return mTitleScenePath.c_str();
			case GameSceneId::Game: return mGameplayScenePath.c_str();
			default: return mGameplayScenePath.c_str();
		}
	}
}
