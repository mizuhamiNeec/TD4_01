#pragma once
#include <memory>

#include <core/assets/AssetID.h>

#include <engine/EngineConfig.h>

class IPostProcess;
class SrvManager;

namespace Unnamed {
	class AudioSystem;
	class EditorRuntime;
	class ImGuiLayer;
	class ConsoleSystem;
	class ConCommand;
	class World;

	namespace Render {
		class RenderModule;
		struct RenderFrameContext;
	}

	namespace Rhi {
		class IRhiDevice;
	}

	/// @brief エンジンクラス
	class Engine {
	public:
		/// @brief コンストラクタ
		Engine();

		/// @brief デストラクタ
		~Engine();

		/// @brief エンジンの実行
		/// @return 終了コード
		int Run();

		/// @brief エディターモードの画面表示モードを切り替えます。
		void ToggleEditorScreenMode() const;

	private:
		/// @brief 初期化処理
		bool Init();
		/// @brief 更新処理
		void Tick();
		/// @brief 終了処理
		void Shutdown();

		/// @brief コンソールコマンドとコンソール変数を登録します。
		void RegisterConsoleCommandsAndVariables();

		/// @brief フルスクリーンの切り替えを行います。
		void ToggleFullscreen() const;

		/// @brief ワールドを切り替えます。
		/// @tparam TWorld 切り替えるワールドの型
		/// @tparam Args コンストラクタに渡す引数の型
		/// @param args コンストラクタに渡す引数
		/// @return 切り替えたワールドの参照
		template <class TWorld, class... Args>
		TWorld& SwitchWorld(Args&&... args);

		/// @brief 現在のワールドを取得します。
		/// @return 現在のワールドの参照
		[[nodiscard]] World* GetWorld() const;

		EngineConfig mConfig;

		// 基本システム
		std::unique_ptr<class AssetManager>       mAssetManager;
		std::unique_ptr<class PlatformEventsImpl> mPlatformEvents;
		std::unique_ptr<class WindowManager>      mWindowManager;

		// 基幹システム
		std::unique_ptr<ConsoleSystem>        mConsoleSystem;
		std::unique_ptr<class TerminalSystem> mTerminalSystem;

		std::unique_ptr<class TimeSystem>  mTimeSystem;
		std::unique_ptr<class InputSystem> mInputSystem;
		std::unique_ptr<class Profiler>    mProfiler;

		std::unique_ptr<Rhi::IRhiDevice>      mRhiDevice;
		std::unique_ptr<Render::RenderModule> mRenderModule;

		std::unique_ptr<World> mWorld;

#ifdef _DEBUG
		std::unique_ptr<ImGuiLayer>    mUImGuiLayer;
		std::unique_ptr<EditorRuntime> mUEditorRuntime;
#endif

		std::unique_ptr<AudioSystem> mAudioSystem;
		std::unique_ptr<ConCommand> mQuitCommand;
		std::unique_ptr<ConCommand> mToggleEditorCommand;
		std::unique_ptr<ConCommand> mToggleFullscreenCommand;


		std::unique_ptr<Render::RenderFrameContext> mRenderFrameContext;
		float mAssetHotReloadPollAccumulator = 0.0f;
		uint32_t mFrameIndex = 0;
		uint32_t mLastResizeWidth = 0;
		uint32_t mLastResizeHeight = 0;

#ifdef _DEBUG
		bool mIsEditorMode = true;
#else
		bool mIsEditorMode = false;
#endif

		bool mWishShutdown = false;
	};
}
