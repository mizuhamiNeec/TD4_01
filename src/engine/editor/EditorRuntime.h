#pragma once
#ifdef _DEBUG

#include "EditorToolHost.h"

namespace Unnamed {
	class WindowManager;
	class ImGuiLayer;
	class World;

	namespace Render {
		class RenderModule;
		struct RenderFrameInputs;
	}

	class EditorRuntime final {
	public:
		EditorRuntime(
			ConsoleSystem*        console,
			WindowManager&        windowManager,
			Render::RenderModule& renderModule,
			ImGuiLayer&           imGuiLayer
		);
		~EditorRuntime();

		void BeginUI();
		void BuildUi(float deltaTime);

		void                              TogglePresentMode();
		[[nodiscard]] EDITOR_PRESENT_MODE GetPresentMode() const;

		void FillEditorRenderViews(Render::RenderFrameInputs& inputs);
		void SyncViewOutputs();

		[[nodiscard]] World* GetRuntimeWorld() const;

	private:
		ConsoleSystem* mConsole;
		EditorToolHost mToolHost;
	};
}

#endif
