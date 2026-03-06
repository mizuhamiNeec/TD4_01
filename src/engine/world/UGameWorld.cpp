#include "UGameWorld.h"

#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/USceneSerializer.h"

#include "engine/unnamed/subsystem/console/Log.h"

#include <chrono>

namespace Unnamed {
	UGameWorld::~UGameWorld() = default;

	void UGameWorld::Initialize() {
		const auto start = std::chrono::steady_clock::now();
		UWorld::Initialize();
		const auto worldInitEnd = std::chrono::steady_clock::now();

		Msg(
			"UGameWorld",
			"Initialize timing: worldInit={}ms",
			std::chrono::duration_cast<std::chrono::milliseconds>(
				worldInitEnd - start
			).count()
		);
	}

	void UGameWorld::Shutdown() { UWorld::Shutdown(); }

	void UGameWorld::Tick(const float deltaTime) { UWorld::Tick(deltaTime); }

	bool UGameWorld::LoadSceneFromFile(const char* path) {
		const auto start = std::chrono::steady_clock::now();
		const bool ok    = [&]() {
			if (!path || std::string_view(path).empty()) { return false; }

			auto       newScene = std::make_unique<UScene>();
			const bool loadOk   = USceneSerializer::LoadFromFile(
				*newScene, path, mGuidGenerator
			);
			if (!loadOk) { return false; }

			const auto beforeUnload = std::chrono::steady_clock::now();
			UnloadScene();
			const auto afterUnload = std::chrono::steady_clock::now();

			UWorld::SetScene(std::move(newScene));
			const auto afterSetScene = std::chrono::steady_clock::now();

			Msg(
				"UGameWorld",
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
				"UGameWorld",
				"LoadSceneFromFile timing: failed total={}ms path={}",
				std::chrono::duration_cast<std::chrono::milliseconds>(
					end - start
				).count(),
				(path ? std::string(path) : std::string("<null>"))
			);
		}
		return ok;
	}

	void UGameWorld::UnloadScene() {
		if (!mScene) { return; }
		UWorld::UnloadScene();
	}

	void UGameWorld::FillRenderFrameInputs(
		Render::RenderFrameInputs&  inputs,
		Render::RenderFrameContext& frameContext,
		AssetManager&               assetManager
	) {
		UWorld::FillRenderFrameInputs(inputs, frameContext, assetManager);
	}

	void UGameWorld::SetScene(std::unique_ptr<UScene> scene) {
		const auto start = std::chrono::steady_clock::now();
		if (mScene) {
			const auto unloadStart = std::chrono::steady_clock::now();
			UnloadScene();
			const auto unloadEnd = std::chrono::steady_clock::now();
			Msg(
				"UGameWorld",
				"SetScene timing: UnloadScene={}ms",
				std::chrono::duration_cast<std::chrono::milliseconds>(
					unloadEnd - unloadStart
				).count()
			);
		}

		const auto setStart = std::chrono::steady_clock::now();
		UWorld::SetScene(std::move(scene));
		const auto setEnd = std::chrono::steady_clock::now();

		Msg(
			"UGameWorld",
			"SetScene timing: setScene={}ms",
			std::chrono::duration_cast<std::chrono::milliseconds>(
				setEnd - setStart
			).count()
		);
	}
}
