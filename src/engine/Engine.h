#pragma once
#include <memory>
#include <engine/EngineConfig.h>
#include <engine/ImGui/ImGuiManager.h>
#include <engine/Line/LineCommon.h>
#include <engine/Model/ModelCommon.h>
#include <engine/Object3D/Object3DCommon.h>
#include <engine/OldConsole/Console.h>
#include <engine/particle/ParticleManager.h>
#include <engine/postprocess/IPostProcess.h>
#include <engine/postprocess/PostProcessPipeline.h>
#include <engine/renderer/D3D12.h>
#include <engine/renderer/RenderTargets.h>
#include <engine/ResourceSystem/Manager/ResourceManager.h>
#include <engine/Sprite/SpriteCommon.h>
#include <engine/unnamed/subsystem/input/UInputSystem.h>
#include <engine/unnamed/subsystem/time/TimeSystem.h>
#include <game/scene/GameScene.h>

#include "Platform/WindowManager.h"

#include "runtime/world/UWorld.h"

class AudioManager;

namespace Unnamed {
	/// @brief エンジンクラス
	class Engine {
	public:
		Engine();
		~Engine();

		int Run();

		[[nodiscard]]
		AudioManager* GetAudioManagerInstance() const {
			return mAudioManager.get();
		}

		[[nodiscard]]
		D3D12* GetRendererInstance() const { return mRenderer.get(); }

		[[nodiscard]]
		ResourceManager* GetResourceManagerInstance() const {
			return mResourceManager.get();
		}

		[[nodiscard]]
		SpriteCommon* GetSpriteCommonInstance() const {
			return mSpriteCommon.get();
		}

		[[nodiscard]]
		ParticleManager* GetParticleManagerInstance() const {
			return mParticleManager.get();
		}

		[[nodiscard]]
		SrvManager* GetSrvManagerInstance() const {
			return mResourceManager->GetSrvManager();
		}

		[[nodiscard]]
		TexManager* GetTexManagerInstance() const {
			return mResourceManager ?
				       mResourceManager->GetTexManager() :
				       nullptr;
		}

		[[nodiscard]]
		Vec2 GetViewportLTInstance() const { return mViewportLT; }

		[[nodiscard]]
		Vec2 GetViewportSizeInstance() const { return mViewportSize; }

		[[nodiscard]]
		float& GetBlurStrengthInstance() { return mBlurStrength; }

		void OnResize(uint32_t width, uint32_t height);
		void ResizeOffscreenRenderTextures(uint32_t width, uint32_t height);

		void RegisterConsoleCommandsAndVariables();

		/// @brief エディターインスタンスの取得
		//Editor* GetEditor() const { return mEditor.get(); }

		/// @brief ポストプロセスエフェクト数の取得
		std::size_t GetPostChainSize() const {
			return mPostProcessPipeline.GetPassCount();
		}

		/// @brief ポストプロセスエフェクトの取得
		IPostProcess* GetPostProcessAt(int index) const {
			if (index < 0) { return nullptr; }
			return mPostProcessPipeline.GetPassAt(static_cast<size_t>(index));
		}

		/// @brief ビューポートをメインウィンドウ全体へ設定
		void SetViewportToMainWindow();

		/// @brief ビューポート(左上座標/サイズ)をエディター UI の値で更新
		void SetViewportFromEditor(float x, float y, float w, float h);

		/// @brief 現在の PingPong テクスチャの SRV (GPU ptr)
		uint64_t GetActivePingSrvGpuPtr() const {
			return mPostProcessPipeline.GetActivePingSrvGpuPtr();
		}

		/// @brief 現在の PingPong テクスチャの RTV リソース記述子
		D3D12_RESOURCE_DESC GetActivePingRtvDesc() const {
			return mPostProcessPipeline.GetActivePingRtvDesc();
		}

	private:
		/// @brief 初期化処理
		bool Init();
		/// @brief 更新処理
		void Tick();
		/// @brief 終了処理
		void Shutdown();

		/// @brief ワールドを切り替えます。
		/// @tparam TWorld 切り替えるワールドの型
		/// @tparam Args コンストラクタに渡す引数の型
		/// @param args コンストラクタに渡す引数
		/// @return 切り替えたワールドの参照
		template <class TWorld, class... Args>
		TWorld& SwitchWorld(Args&&... args) {
			static_assert(std::is_base_of_v<UWorld, TWorld>);

			if (mWorld) {
				mWorld->Shutdown();
				mWorld.reset();
			}

			auto newWorld = std::make_unique<TWorld>(
				std::forward<Args>(args)...
			);
			TWorld* raw = newWorld.get();

			mWorld = std::move(newWorld);

			mWorld->Initialize();
			return *raw;
		}

		/// @brief 現在のワールドを取得します。
		/// @return 現在のワールドの参照
		UWorld* GetWorld() const { return mWorld.get(); }

	private:
		EngineConfig mConfig;

		// 基幹システム
		std::unique_ptr<ConsoleSystem> mConsoleSystem;
		std::unique_ptr<TimeSystem>    mTimeSystem;

		// 基本システム
		std::unique_ptr<WindowManager> mWindowManager;
		std::unique_ptr<UInputSystem>  mInputSystem;

		std::unique_ptr<UWorld> mWorld;

		std::unique_ptr<Console> mConsole;
		PostProcessPipeline      mPostProcessPipeline;
		RenderTargets            mRenderTargets;
		bool                     mSwapchainPassBegun = false;


		std::unique_ptr<ResourceManager> mResourceManager;

		std::unique_ptr<D3D12> mRenderer;

#ifdef _DEBUG
		std::unique_ptr<ImGuiManager> mImGuiManager;
#endif

		std::unique_ptr<ParticleManager> mParticleManager;
		std::unique_ptr<SpriteCommon>    mSpriteCommon;
		std::unique_ptr<Object3DCommon>  mObject3DCommon;
		std::unique_ptr<ModelCommon>     mModelCommon;
		std::unique_ptr<LineCommon>      mLineCommon;
		std::unique_ptr<AudioManager>    mAudioManager;

		Vec2 mViewportLT;
		Vec2 mViewportSize;

#ifdef _DEBUG
		bool mIsEditorMode = true;
#else
		bool mIsEditorMode = false;
#endif

		bool mWishShutdown = false;

		float mBlurStrength = 0.0f;
	};
}
