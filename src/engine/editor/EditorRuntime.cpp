#ifdef _DEBUG
#include "EditorRuntime.h"

namespace Unnamed {
	EditorRuntime::EditorRuntime(
		ConsoleSystem*        console,
		InputSystem*          inputSystem,
		AssetManager*         assetManager,
		DemoManager*          demoManager,
		Profiler*             profiler,
		WindowManager&        windowManager,
		Render::RenderModule& renderModule,
		ImGuiLayer&           imGuiLayer
	) : mConsole(console),
	    mToolHost(
		    console,
		    inputSystem,
		    assetManager,
		    demoManager,
		    profiler,
		    windowManager,
		    renderModule,
		    imGuiLayer
	    ) {
		mToolHost.Initialize();
	}

	EditorRuntime::~EditorRuntime() = default;

	void EditorRuntime::BeginUI() {
		mToolHost.BeginUI();
	}

	void EditorRuntime::BuildUi(const float deltaTime) {
		const EditorToolFrameContext frameContext = {
			.unscaledDeltaTime = deltaTime,
			.deltaTime         = deltaTime,
		};
		mToolHost.Tick(frameContext);
		mToolHost.BuildUi(frameContext);
	}

	void EditorRuntime::TogglePresentMode() {
		mToolHost.TogglePresentMode();
	}

	EDITOR_PRESENT_MODE EditorRuntime::GetPresentMode() const {
		return mToolHost.GetPresentMode();
	}

	void EditorRuntime::FillEditorRenderViews(
		Render::RenderFrameInputs& inputs
	) {
		mToolHost.CollectRenderViews(inputs);
	}

	void EditorRuntime::SyncViewOutputs() {
		mToolHost.SyncViewOutputs();
	}

	World* EditorRuntime::GetRuntimeWorld() const {
		return mToolHost.GetRuntimeWorld();
	}
}

#endif
