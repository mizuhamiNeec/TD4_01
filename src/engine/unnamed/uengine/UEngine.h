#pragma once
#include <memory>
#include <vector>

#include <engine/platform/PlatformEventsImpl.h>
#include <engine/unnamed/subsystem/interface/ISubsystem.h>
#include <engine/unnamed/subsystem/render/URenderSubsystem.h>
#include <engine/unnamed/subsystem/window/Win32/Win32WindowSystem.h>
#include <engine/unnamed/urenderer/GraphicsDevice.h>
#include <engine/unnamed/urootsignaturecache/RootSignatureCache.h>
#include <engine/unnamed/uuploadarena/UploadArena.h>
#include <game/gameframework/world/UWorld.h>

class TimeSystem;

namespace Unnamed {
	class UInputSystem;

	/// @brief エンジンクラス
	class UEngine {
	public:
		/// @brief コンストラクタ
		UEngine();

		/// @brief デストラクタ
		~UEngine();

		/// @brief メインループを実行します
		void Run();

	private:
		/// @brief 初期化
		void Init();

		/// @brief メインループ
		void Tick();

		/// @brief シャットダウン
		void Shutdown() const;

		// Subsystems
		std::vector<std::unique_ptr<ISubsystem>> mSubsystems;
		ConsoleSystem*                           mConsole;
		TimeSystem*                              mTime;
		Win32WindowSystem*                       mWindowSystem;
		UInputSystem*                            mInputSystem;
		URenderSubsystem*                        mRenderer;

		// Managers and Systems
		std::unique_ptr<UAssetManager>         mAssetManager;
		std::unique_ptr<GraphicsDevice>        mGraphicsDevice;
		std::unique_ptr<PlatformEventsImpl>    mPlatformEvents;
		std::unique_ptr<RootSignatureCache>    mRootSignatureCache;
		std::unique_ptr<RenderResourceManager> mRenderResourceManager;
		std::unique_ptr<ShaderLibrary>         mShaderLibrary;
		std::unique_ptr<UPipelineCache>        mPipelineCache;
		std::unique_ptr<UploadArena>           mUploadArena;

		// TODO: 削除予定
		std::unique_ptr<UWorld> mWorld;
		uint32_t                mMaterialAsset;
		TransformComponent*     mCameraTransform = nullptr;
		UCameraComponent*       mCamera          = nullptr;
	};
}
