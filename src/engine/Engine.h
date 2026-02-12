#pragma once
#include <memory>

#include <core/assets/AssetID.h>

#include <engine/EngineConfig.h>
#include <engine/postprocess/PostProcessPipeline.h>
#include <engine/renderer/RenderTargets.h>

class ImGuiManager;
class ModelCommon;
class Object3DCommon;
class LineCommon;
class ParticleManager;
class SpriteCommon;
class AudioManager;
class TexManager;
class ResourceManager;

namespace Unnamed {
	class ConsoleSystem;
	class ConVarHelper;
	class UWorld;

	namespace Rhi {
		class IRhiDevice;
	}

	namespace Render {
		class RenderGraph;
	}

	/// @brief エンジンクラス
	class Engine {
	public:
		Engine();
		~Engine();

		int Run();

		[[nodiscard]]
		AudioManager* GetAudioManagerInstance() const;

		[[nodiscard]]
		D3D12* GetRendererInstance() const;

		[[nodiscard]]
		ResourceManager* GetResourceManagerInstance() const;

		[[nodiscard]]
		SpriteCommon* GetSpriteCommonInstance() const;

		[[nodiscard]]
		ParticleManager* GetParticleManagerInstance() const;

		[[nodiscard]]
		SrvManager* GetSrvManagerInstance() const;

		[[nodiscard]]
		TexManager* GetTexManagerInstance() const;

		[[nodiscard]]
		Vec2 GetViewportLTInstance() const;

		[[nodiscard]]
		Vec2 GetViewportSizeInstance() const;

		[[nodiscard]]
		float& GetBlurStrengthInstance();

		void OnResize(uint32_t width, uint32_t height);
		void ResizeOffscreenRenderTextures(uint32_t width, uint32_t height);

		void RegisterConsoleCommandsAndVariables();

		/// @brief エディターインスタンスの取得
		//Editor* GetEditor() const { return mEditor.get(); }

		/// @brief ポストプロセスエフェクト数の取得
		std::size_t GetPostChainSize() const;

		/// @brief ポストプロセスエフェクトの取得
		IPostProcess* GetPostProcessAt(int index) const;

		/// @brief 現在の PingPong テクスチャの SRV (GPU ptr)
		uint64_t GetActivePingSrvGpuPtr() const;

		/// @brief 現在の PingPong テクスチャの RTV リソース記述子
		D3D12_RESOURCE_DESC GetActivePingRtvDesc() const;

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
		TWorld& SwitchWorld(Args&&... args);

		/// @brief 現在のワールドを取得します。
		/// @return 現在のワールドの参照
		UWorld* GetWorld() const;

		EngineConfig mConfig;

		// 基本システム
		std::unique_ptr<class AssetManager>       mAssetManager;
		std::unique_ptr<class PlatformEventsImpl> mPlatformEvents;
		std::unique_ptr<class WindowManager>      mWindowManager;

		// 基幹システム
		std::unique_ptr<ConsoleSystem>        mConsoleSystem;
		std::unique_ptr<class TerminalSystem> mTerminalSystem;
		std::unique_ptr<ConVarHelper>         mConVarHelper;

		std::unique_ptr<class TimeSystem>   mTimeSystem;
		std::unique_ptr<class UInputSystem> mInputSystem;

		std::unique_ptr<Rhi::IRhiDevice>     mRhiDevice;
		std::unique_ptr<Render::RenderGraph> mGraph;

		AssetID test = kInvalidAssetID;

		std::unique_ptr<UWorld> mWorld;

		PostProcessPipeline mPostProcessPipeline;
		RenderTargets       mRenderTargets;
		bool                mSwapchainPassBegun = false;

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
