#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <editor/Editor.h>

#include <engine/EngineConfig.h>
#include <engine/CopyImagePass/CopyImagePass.h>
#include <engine/Entity/EntityLoader.h>
#include <engine/ImGui/ImGuiManager.h>
#include <engine/Line/LineCommon.h>
#include <engine/Model/ModelCommon.h>
#include <engine/Object3D/Object3DCommon.h>
#include <engine/OldConsole/Console.h>
#include <engine/particle/ParticleManager.h>
#include <engine/postprocess/IPostProcess.h>
#include <engine/renderer/D3D12.h>
#include <engine/ResourceSystem/Manager/ResourceManager.h>
#include <engine/SceneManager/SceneFactory.h>
#include <engine/SceneManager/SceneManager.h>
#include <engine/Sprite/SpriteCommon.h>
#include <engine/unnamed/subsystem/time/TimeSystem.h>
#include <engine/Window/WindowManager.h>

#include <game/scene/GameScene.h>

#include "editor/UEditor.h"

#include "game/gameframework/map/Map.h"

#include "unnamed/subsystem/input/UInputSystem.h"

class AudioManager;

namespace Unnamed {
	/// @brief エンジンクラス
	class Engine {
	public:
		Engine();
		~Engine();

		int Run();

	public:
		//DEPRECATED: 旧エンジンクラス
		static AudioManager* GetAudioManager() {
			return mAudioManager.get();
		}

		// DEPRECATED: 旧エンジンクラス
		static D3D12* GetRenderer() {
			return mRenderer.get();
		}

		// DEPRECATED: 旧エンジンクラス
		static ResourceManager* GetResourceManager() {
			return mResourceManager.get();
		}

		// DEPRECATED: 旧エンジンクラス
		static SpriteCommon* GetSpriteCommon() {
			return mSpriteCommon.get();
		}

		// DEPRECATED: 旧エンジンクラス
		static ParticleManager* GetParticleManager() {
			return mParticleManager.get();
		}

		// DEPRECATED: 旧エンジンクラス
		static SrvManager* GetSrvManager() {
			return mSrvManager.get();
		}

		// DEPRECATED: 旧エンジンクラス
		static SceneManager* GetSceneManager() {
			return mSceneManager.get();
		}

		// DEPRECATED: 旧エンジンクラス
		static Vec2 GetViewportLT() {
			return mViewportLT;
		}

		// DEPRECATED: 旧エンジンクラス
		static Vec2 GetViewportSize() {
			return mViewportSize;
		}

		// DEPRECATED: 旧エンジンクラス
		static float blurStrength;

		void OnResize(uint32_t width, uint32_t height);
		void ResizeOffscreenRenderTextures(uint32_t width, uint32_t height);

		static void RegisterConsoleCommandsAndVariables();
		static void Quit(const std::vector<std::string>& args = {});
		void        CheckEditorMode();

	private:
		bool Init();
		void Tick();
		void Shutdown() const;

	private:
		EngineConfig mConfig;

		std::unique_ptr<ConsoleSystem> mConsoleSystem;
		std::unique_ptr<TimeSystem>    mTimeSystem;
		std::unique_ptr<UInputSystem>  mInputSystem;
		std::unique_ptr<UEditor>       mUEditor;
		std::unique_ptr<Map>           mMap;

		std::unique_ptr<OldWindowManager> mWindowManager;
		std::unique_ptr<Editor> mEditor;
		std::unique_ptr<EntityLoader> mEntityLoader;
		std::unique_ptr<Console> mConsole;
		std::unique_ptr<CopyImagePass> mCopyImagePass;
		std::vector<std::unique_ptr<IPostProcess>> mPostChain;
		RenderTargetTexture mOffscreenRtv;
		DepthStencilTexture mOffscreenDsv;
		RenderPassTargets mOffscreenRenderPassTargets;
		RenderTargetTexture mPostProcessedRtv;
		DepthStencilTexture mPostProcessedDsv;
		RenderPassTargets mPostProcessedRenderPassTargets;
		RenderTargetTexture mPingRtv[2];
		uint32_t mPingIndex = 0;
		bool bSwapchainPassBegun = false;

		static std::unique_ptr<SrvManager>      mSrvManager;
		static std::unique_ptr<ResourceManager> mResourceManager;

		static std::unique_ptr<D3D12> mRenderer;

#ifdef _DEBUG
		std::unique_ptr<ImGuiManager> mImGuiManager;
#endif

		static std::unique_ptr<ParticleManager> mParticleManager;

		static std::unique_ptr<SpriteCommon> mSpriteCommon;
		std::unique_ptr<Object3DCommon>      mObject3DCommon;
		std::unique_ptr<ModelCommon>         mModelCommon;
		std::unique_ptr<LineCommon>          mLineCommon;
		std::unique_ptr<SceneFactory>        mSceneFactory;
		static std::unique_ptr<AudioManager> mAudioManager;

		static std::shared_ptr<SceneManager> mSceneManager;
		static std::optional<std::string>    mPendingSceneChange;

		static Vec2 mViewportLT;
		static Vec2 mViewportSize;
		static bool mIsEditorMode;
		static bool mWishShutdown;
	};
}
