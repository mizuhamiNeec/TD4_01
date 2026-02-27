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

	enum class EDITOR_PRESENT_MODE : uint8_t {
		VIEWPORT_PANEL,
		FULLSCREEN_SWAP_CHAIN,
	};

	enum class EDITOR_VIEWPORT_RENDER_MODE : uint8_t {
		FIT_VIEWPORT,
		FIXED_ASPECT_16_9,
		HD720,
		FHD1080,
	};

	class UEditorRuntime {
	public:
		UEditorRuntime(
			UEditorWorld&         editorWorld,
			WindowManager&        windowManager,
			Render::RenderModule& renderModule,
			UImGuiLayer&          imGuiLayer
		);

		/// @brief この関数の後からImGuiを呼びだしてください。
		void BeginUI();

		/// @brief UIの構築と描画を行います。
		void BuildUi();

		/// @brief シーンのレンダリング結果を設定します。
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

		[[nodiscard]] UEntity* GetSelectedEntity() const;

		bool SaveSceneAs(const std::string& path);

		UEditorWorld&         mEditorWorld;
		WindowManager&        mWindowManager;
		Render::RenderModule& mRenderModule;
		UImGuiLayer&          mImGuiLayer;

		EntityId mSelectedEntityId = 0;

		EDITOR_PRESENT_MODE mPresentMode = EDITOR_PRESENT_MODE::VIEWPORT_PANEL;
		EDITOR_VIEWPORT_RENDER_MODE mViewportRenderMode =
			EDITOR_VIEWPORT_RENDER_MODE::HD720;
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
