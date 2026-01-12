#pragma once
#include <memory>
#include <vector>

#include <engine/unnamed/gameframework/world/UWorld.h>
#include <engine/platform/PlatformEventsImpl.h>
#include <engine/unnamed/subsystem/interface/ISubsystem.h>
#include <engine/unnamed/subsystem/render/URenderSubsystem.h>
#include <engine/unnamed/subsystem/window/Win32/Win32WindowSystem.h>
#include <engine/unnamed/urenderer/GraphicsDevice.h>
#include <engine/unnamed/urootsignaturecache/RootSignatureCache.h>
#include <engine/unnamed/uuploadarena/UploadArena.h>

#include "engine/unnamed/gameframework/ecs/ECSEntity.h"
#include "engine/unnamed/gameframework/ecs/World.h"

class TimeSystem;

namespace Unnamed {
	class UInputSystem;

	/// @brief エンジンクラス
	class UEngine {
	public:
		UEngine();
		~UEngine();

		void Run();

	private:
		void Init();
		void Tick();
		void Shutdown() const;

	private:
		std::vector<std::unique_ptr<ISubsystem>> mSubsystems;
		ConsoleSystem*                           mConsole;
		TimeSystem*                              mTime;
		Win32WindowSystem*                       mWindowSystem;
		UInputSystem*                            mInputSystem;
		URenderSubsystem*                        mRenderer;

		std::unique_ptr<PlatformEventsImpl> mPlatformEvents;

		std::unique_ptr<UAssetManager>         mAssetManager;
		std::unique_ptr<GraphicsDevice>        mGraphicsDevice;
		std::unique_ptr<RenderResourceManager> mRenderResourceManager;
		std::unique_ptr<ShaderLibrary>         mShaderLibrary;
		std::unique_ptr<RootSignatureCache>    mRootSignatureCache;
		std::unique_ptr<UPipelineCache>        mPipelineCache;
		std::unique_ptr<UploadArena>           mUploadArena;

		std::unique_ptr<UWorld> mWorld;

		ECS::World  mECSWorld;
		ECS::Entity mEntity;


		uint32_t mMaterialAsset;

		TransformComponent* mCameraTransform = nullptr;
		UCameraComponent*   mCamera          = nullptr;
	};
}
