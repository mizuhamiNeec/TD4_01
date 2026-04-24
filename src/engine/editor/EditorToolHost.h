#pragma once
#pragma once
#ifdef _DEBUG

#include <memory>
#include <vector>

#include "IEditorTool.h"
#include "LevelEditorTool.h"

namespace Unnamed {
	class ConsoleSystem;
	class InputSystem;
	class AssetManager;
	class IDemoService;
	class IGameModule;
	class Profiler;
	class WindowManager;
	class ImGuiLayer;

	namespace Render {
		class RenderModule;
		struct RenderFrameInputs;
		struct SceneOutputView;
	}

	class GuiEditorTool;

	class EditorToolHost final {
	public:
		EditorToolHost(
			ConsoleSystem*        console,
			InputSystem*          inputSystem,
			AssetManager*         assetManager,
			IDemoService*         demoService,
			IGameModule&          gameModule,
			Profiler*             profiler,
			WindowManager&        windowManager,
			Render::RenderModule& renderModule,
			ImGuiLayer&           imGuiLayer
		);
		~EditorToolHost();

		void Initialize() const;
		void Shutdown();
		void BeginUI() const;
		void Tick(const EditorToolFrameContext& frameContext) const;
		void BuildUi(const EditorToolFrameContext& frameContext);
		void CollectRenderViews(Render::RenderFrameInputs& inputs) const;
		void SyncViewOutputs() const;

		[[nodiscard]] World* GetRuntimeWorld() const;

		void                              TogglePresentMode() const;
		[[nodiscard]] EDITOR_PRESENT_MODE GetPresentMode() const;

	private:
		void RegisterTool(std::unique_ptr<IEditorTool> tool);

		WindowManager&        mWindowManager;
		Render::RenderModule& mRenderModule;
		ImGuiLayer&           mImGuiLayer;

		ConsoleSystem* mConsole = nullptr;
		InputSystem*   mInputSystem = nullptr;
		AssetManager*  mAssetManager = nullptr;
		IDemoService*  mDemoService = nullptr;
		IGameModule&   mGameModule;
		Profiler*      mProfiler = nullptr;

		std::vector<std::unique_ptr<IEditorTool>> mOwnedTools;
		LevelEditorTool*                          mLevelTool = nullptr;
		bool                                      mMainDockInitialized = false;
	};
}

#endif
