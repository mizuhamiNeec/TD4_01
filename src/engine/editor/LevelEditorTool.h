#pragma once
#ifdef _DEBUG

#include <array>
#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "EditorNotification.h"
#include "EditorViewportCameraManager.h"
#include "IEditorTool.h"

#include "core/math/Vec2.h"

#include "engine/Properties.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/Scene.h"

namespace Unnamed {
	class WindowManager;
	class EditorWorld;
	class ImGuiLayer;

	namespace Render {
		struct SceneOutputView;
	}

	enum class EDITOR_PRESENT_MODE : uint8_t {
		VIEWPORT_PANEL,
		FULLSCREEN_SWAP_CHAIN,
	};

	enum class EDITOR_VIEWPORT_RENDER_MODE : uint8_t {
		FIT_VIEWPORT,
		FIXED_ASPECT_16_9,
		FIXED_ASPECT_4_3,
		HD720,
		FHD1080,
	};

	enum class EDITOR_VIEW_LAYOUT_MODE : uint8_t {
		SINGLE = 0,
		QUAD   = 1,
	};

	class LevelEditorTool final : public IEditorTool {
	public:
		LevelEditorTool(WindowManager& windowManager, ImGuiLayer& imGuiLayer);
		~LevelEditorTool() override;

		void Initialize(const EditorToolServices& services) override;
		void Shutdown() override;
		void BeginUI() override;
		void Tick(const EditorToolFrameContext& frameContext) override;
		void BuildUi(const EditorToolFrameContext& frameContext) override;
		void CollectRenderViews(Render::RenderFrameInputs& inputs) override;
		void EnumerateViewKeys(std::vector<std::string>& outViewKeys) const override;
		void SetViewOutput(
			std::string_view viewKey,
			const Render::SceneOutputView& output,
			Vec2 size
		) override;
		[[nodiscard]] std::string_view GetToolId() const override;
		[[nodiscard]] std::string_view GetDisplayName() const override;
		[[nodiscard]] World* GetRuntimeWorld() override;
		[[nodiscard]] bool IsOpen() const override;
		void SetOpen(bool open) override;

		void TogglePresentMode();
		[[nodiscard]] EDITOR_PRESENT_MODE GetPresentMode() const;
		[[nodiscard]] bool IsPlaying() const;
		void StartPlayInEditor();
		void StopPlayInEditor();
		[[nodiscard]] bool IsProfilerWindowOpen() const;
		void SetProfilerWindowOpen(bool open);

		static constexpr std::string_view kViewScenePerspective =
			"tool.level.scene.perspective";
		static constexpr std::string_view kViewSceneTop =
			"tool.level.scene.top";
		static constexpr std::string_view kViewSceneFront =
			"tool.level.scene.front";
		static constexpr std::string_view kViewSceneRight =
			"tool.level.scene.right";

	private:
		struct ViewOutputCache {
			D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = {};
			uint64_t srvRevision = 0;
			uint32_t textureId = 0;
			Vec2 size = Vec2::zero;
		};

		static void LoadImGuizmoSettings();

		[[nodiscard]] Render::SceneViewRenderMode BuildSceneViewModeForSize(
			float width,
			float height,
			bool forceFit = false
		) const;
		[[nodiscard]] Scene* GetOutlinerScene();
		[[nodiscard]] const Scene* GetOutlinerScene() const;

		void DrawMainMenu();
		void DrawSideBar();
		void DrawStatusBar();

		void DrawViewport(float deltaTime);
		void DrawViewportTopBar();
		void DrawViewportGizmo(
			std::string_view viewKey,
			const Render::SceneViewRenderMode& sceneViewMode,
			const Vec2& imagePos,
			float drawWidth,
			float drawHeight
		);
		void DrawViewportOverlay(float deltaTime);

		void DrawSceneOutliner();
		void DrawInspector();
		void DrawProfilerWindow();

		[[nodiscard]] Entity* GetSelectedEntity() const;
		bool SaveSceneAs(const std::string& path);

		[[nodiscard]] ViewportCameraBinding ResolveViewportBinding(
			std::string_view viewKey
		) const;

		std::unique_ptr<EditorWorld> mOwnedEditorWorld;
		EditorWorld& mEditorWorld;
		WindowManager& mWindowManager;
		ImGuiLayer& mImGuiLayer;

		EntityId mSelectedEntityId = 0;
		bool mOpen = true;
		bool mInitialized = false;
		bool mDockInitialized = false;

		EDITOR_PRESENT_MODE mPresentMode = EDITOR_PRESENT_MODE::VIEWPORT_PANEL;
		EDITOR_VIEWPORT_RENDER_MODE mViewportRenderMode =
			EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT;
		EDITOR_VIEW_LAYOUT_MODE mViewportLayoutMode = EDITOR_VIEW_LAYOUT_MODE::SINGLE;
		std::string mActiveViewportViewKey = std::string(kViewScenePerspective);
		Vec2 mViewportPosition = Vec2::zero;
		Vec2 mViewportSize = Vec2::zero;
		Vec2 mLastViewportSize = Vec2(kClientWidth, kClientHeight);
		float mViewportPanelWidth = kClientWidth;
		float mViewportPanelHeight = kClientHeight;
		bool mViewportSizeChangedThisFrame = false;
		EditorViewportCameraManager mCameraManager = {};
		std::unordered_map<std::string, ViewOutputCache> mViewOutputs;

		std::unique_ptr<EditorNotification> mNotification;

		float mGridSnap = 64.0f;
		float mAngleSnapDegree = 15.0f;
		float mCameraSpeedPopupTimer = 0.0f;
		bool mShowProfilerWindow = false;
		bool mViewportLookActive = false;
	};
}

#endif
