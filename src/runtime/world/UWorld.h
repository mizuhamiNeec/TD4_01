#pragma once
#include <memory>

#include "core/guidgenerator/GuidGenerator.h"
#include <runtime/scene/UScene.h>

namespace Unnamed {

	class JsonReader;

	struct WorldTime {
		float deltaTime         = 0.0f;
		float unscaledDeltaTime = 0.0f;
		float timeSeconds       = 0.0f;
	};

	class UWorld {
	public:
		virtual ~UWorld();

		[[nodiscard]] UScene&       GetScene() { return *mScene; }
		[[nodiscard]] const UScene& GetScene() const { return *mScene; }

		virtual void Initialize();
		virtual void Shutdown();
		virtual void Tick(float deltaTime);

		virtual bool LoadSceneFromFile(const char* path);
		virtual void UnloadScene();

	protected:
		virtual void OnSceneLoaded();
		virtual void OnSceneUnloaded();

	protected:
		std::unique_ptr<UScene> mScene;
		GuidGenerator           mGuidGenerator;
		WorldTime               mTime;
	};
}
