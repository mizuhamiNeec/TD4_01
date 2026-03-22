#include "GameWorld.h"

#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/SceneSerializer.h"

#include "engine/unnamed/subsystem/console/Log.h"

#include <chrono>

#include "engine/scene/Scene.h"

namespace Unnamed {
	GameWorld::~GameWorld() = default;

	void GameWorld::Initialize() {
		const auto start = std::chrono::steady_clock::now();
		World::Initialize();
		const auto worldInitEnd = std::chrono::steady_clock::now();

		Msg(
			"GameWorld",
			"Initialize timing: worldInit={}ms",
			std::chrono::duration_cast<std::chrono::milliseconds>(
				worldInitEnd - start
			).count()
		);
	}

	void GameWorld::Shutdown() {
		World::Shutdown();
	}

	void GameWorld::Tick(const float unscaledDeltaTime, const float deltaTime) {
		World::Tick(unscaledDeltaTime, deltaTime);
	}

	bool GameWorld::LoadSceneFromFile(const char* path) {
		const auto start = std::chrono::steady_clock::now();
		const bool ok    = [&] {
			if (!path || std::string_view(path).empty()) {
				return false;
			}

			auto       newScene = std::make_unique<Scene>();
			newScene->SetWorld(this);
			const bool loadOk   = SceneSerializer::LoadFromFile(
				*newScene, path, mGuidGenerator
			);
			if (!loadOk) {
				return false;
			}

			const auto beforeUnload = std::chrono::steady_clock::now();
			UnloadScene();
			const auto afterUnload = std::chrono::steady_clock::now();

			World::SetScene(std::move(newScene));
			const auto afterSetScene = std::chrono::steady_clock::now();

			Msg(
				"GameWorld",
				"LoadSceneFromFile timing: load={}ms unload={}ms setScene={}ms total={}ms path={}",
				std::chrono::duration_cast<std::chrono::milliseconds>(
					beforeUnload - start
				).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(
					afterUnload - beforeUnload
				).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(
					afterSetScene - afterUnload
				).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(
					afterSetScene - start
				).count(),
				std::string(path)
			);

			return true;
		}();

		if (!ok) {
			const auto end = std::chrono::steady_clock::now();
			Msg(
				"GameWorld",
				"LoadSceneFromFile timing: failed total={}ms path={}",
				std::chrono::duration_cast<std::chrono::milliseconds>(
					end - start
				).count(),
				path ? std::string(path) : std::string("<null>")
			);
		}
		return ok;
	}

	void GameWorld::UnloadScene() {
		if (!mScene) {
			return;
		}
		World::UnloadScene();
	}

	void GameWorld::FillRenderFrameInputs(
		Render::RenderFrameInputs&  inputs,
		Render::RenderFrameContext& frameContext,
		AssetManager&               assetManager
	) {
		World::FillRenderFrameInputs(inputs, frameContext, assetManager);
	}

	void GameWorld::SetScene(std::unique_ptr<Scene> scene) {
		const auto start = std::chrono::steady_clock::now();
		if (mScene) {
			const auto unloadStart = std::chrono::steady_clock::now();
			UnloadScene();
			const auto unloadEnd = std::chrono::steady_clock::now();
			Msg(
				"GameWorld",
				"SetScene timing: UnloadScene={}ms",
				std::chrono::duration_cast<std::chrono::milliseconds>(
					unloadEnd - unloadStart
				).count()
			);
		}

		const auto setStart = std::chrono::steady_clock::now();
		World::SetScene(std::move(scene));
		const auto setEnd = std::chrono::steady_clock::now();

		Msg(
			"GameWorld",
			"SetScene timing: setScene={}ms",
			std::chrono::duration_cast<std::chrono::milliseconds>(
				setEnd - setStart
			).count()
		);
	}
}
