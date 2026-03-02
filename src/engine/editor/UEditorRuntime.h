#pragma once
#ifdef _DEBUG

#include <cstdint>
#include <d3d12.h>
#include <string>

#include "core/math/Vec2.h"

#include "engine/Properties.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/UScene.h"

namespace Unnamed {
	class WindowManager;
	class UEditorWorld;
	class UImGuiLayer;

	namespace Render {
		class RenderModule;
		struct SceneOutputView;
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
		void                                     BuildUi();
		[[nodiscard]] Render::SceneRenderRequest GetSceneRenderRequest() const;
		void                                     TogglePresentMode();

		[[nodiscard]] EDITOR_PRESENT_MODE GetPresentMode() const {
			return mPresentMode;
		}

		/// @brief シーンのレンダリング結果を設定します。
		void SetSceneOutput(
			const Render::SceneOutputView& sceneOutput,
			Vec2                           size
		);

	private:
		[[nodiscard]] Render::SceneRenderRequest
		BuildSceneRenderRequest() const;
		[[nodiscard]] UScene*       GetHierarchyScene();
		[[nodiscard]] const UScene* GetHierarchyScene() const;
		void                        DrawViewportGizmo(
			const Render::SceneRenderRequest& sceneRequest,
			const Vec2&                       imagePos,
			float                             drawWidth,
			float                             drawHeight
		);
		void DrawMainMenu();
		void DrawViewportTopBar();
		void DrawSideBar();
		void DrawStatusBar();
		void DrawSceneHierarchy();
		void DrawInspector();
		void DrawProfilerWindow();
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
			EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT;
		Render::SceneRenderRequest mLastSceneRenderRequest = {};
		Vec2 mViewportPosition = Vec2::zero;
		Vec2 mViewportSize = Vec2::zero;
		Vec2 mLastViewportSize = Vec2(kClientWidth, kClientHeight);
		float mViewportPanelWidth = kClientWidth;
		float mViewportPanelHeight = kClientHeight;
		bool mViewportSizeChangedThisFrame = false;

		D3D12_CPU_DESCRIPTOR_HANDLE mSceneSrvCpu = {};
		uint64_t mSceneSrvRevision = 0;
		Vec2 mSceneSize = Vec2(kClientWidth, kClientHeight);
		uint32_t mSceneTextureId = 0;

		float mGridSnap           = 64.0f;
		float mAngleSnapDegree    = 15.0f;
		bool  mShowProfilerWindow = false;
		bool  mViewportLookActive = false;
	};
}

#endif
