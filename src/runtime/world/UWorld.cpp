#include "UWorld.h"

#include "engine/unnamed/subsystem/console/Log.h"

#include "runtime/scene/UScene.h"
#include "runtime/scene/USceneSerializer.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "UWorld";

	UWorld::~UWorld() = default;

	void UWorld::Initialize() {}
	void UWorld::Shutdown() { UnloadScene(); }

	void UWorld::Tick(float deltaTime) {
		deltaTime;
		if (!mScene) { return; }

		// DO SOMETHING	
	}

	bool UWorld::LoadSceneFromFile(const char* path) {
		UnloadScene();                       // 既存のシーンをアンロード
		mScene = std::make_unique<UScene>(); // 新しいシーンを作成

		const bool ok = USceneSerializer::LoadFromFile(
			*mScene, path, mGuidGenerator
		);

		if (!ok) {
			mScene.reset();
			return false;
		}

		OnSceneLoaded();

		Msg(
			kChannel, "Scene loaded successfully from file: {}",
			std::string(path)
		);

		return true;
	}

	void UWorld::UnloadScene() {
		if (!mScene) { return; }
		
		OnSceneUnloaded();
		mScene.reset();
	}

	void UWorld::OnSceneLoaded() {}
	void UWorld::OnSceneUnloaded() {}
}
