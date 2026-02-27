#pragma once
#ifdef _DEBUG

#include <cstdint>
#include <d3d12.h>
#include <string>

#include "core/math/Vec2.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/UScene.h"

namespace Unnamed {
	class WindowManager;
	class UEditorWorld;
	class UImGuiLayer;

	namespace Render {
		class RenderModule;
	}

	enum class EditorPresentMode : uint8_t {
		ViewportPanel,
		FullscreenSwapChain,
	};

	enum class EditorViewportRenderMode : uint8_t {
		FitViewport,
		FixedAspect16x9,
		HD720,
		FHD1080,
	};

	class UEditorRuntime {
	public:
		UEditorRuntime(
			UEditorWorld&         editorWorld,
			WindowManager&        windowManager,
			Render::RenderModule& renderModule,
			UImGuiLayer&          imguiLayer
		);

		void BuildUi();
		void SetSceneOutput(
			uint32_t                    textureId,
			D3D12_CPU_DESCRIPTOR_HANDLE srvCpu,
			Vec2                        size
		);

	private:
		void DrawMainMenu();
		void DrawToolbar();
		void DrawSceneHierarchy();
		void DrawInspector();
		void DrawViewport();

		UEntity* GetSelectedEntity() const;

		bool SaveSceneAs(const std::string& path);

		UEditorWorld&         mEditorWorld;
		WindowManager&        mWindowManager;
		Render::RenderModule& mRenderModule;
		UImGuiLayer&          mImGuiLayer;

		EntityId mSelectedEntityId = 0;

		EditorPresentMode mPresentMode = EditorPresentMode::ViewportPanel;
		EditorViewportRenderMode mViewportRenderMode =
			EditorViewportRenderMode::HD720;
		bool                       mShowDemo                   = true;
		float                      mViewportPanelWidth         = 1280.0f;
		float                      mViewportPanelHeight        = 720.0f;
		bool                       mSkipViewportImageThisFrame = true;
		Render::SceneRenderRequest mLastSceneRenderRequest     = {};

		uint32_t                    mSceneTextureId = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE mSceneSrvCpu    = {};
		Vec2                        mSceneSize      = Vec2(1280.0f, 720.0f);
	};
}

#endif
