#pragma once
#ifdef _DEBUG

#include <cstdint>
#include <d3d12.h>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "EditorNotification.h"
#include "EditorViewportCameraManager.h"

#include "core/math/Vec2.h"

#include "engine/gui/UiDocumentManager.h"
#include "engine/gui/UiDrawCommand.h"
#include "engine/gui/UiRoot.h"
#include "engine/gui/UiScreenStack.h"
#include "engine/gui/editor/GuiEditor.h"

#include "engine/Properties.h"
#include "engine/render/frame/RenderFrameInputs.h"
#include "engine/scene/Scene.h"

namespace Unnamed {
	class WindowManager;
	class EditorWorld;
	class ImGuiLayer;

	namespace Render {
		class RenderModule;
		struct SceneOutputView;
	}

	enum class EDITOR_PRESENT_MODE : uint8_t {
		VIEWPORT_PANEL,        // ビューポートパネルに描画
		FULLSCREEN_SWAP_CHAIN, // フルスクリーンでスワップチェーンに直接描画
	};

	enum class EDITOR_VIEWPORT_RENDER_MODE : uint8_t {
		FIT_VIEWPORT,      // ビューポートに合わせて描画
		FIXED_ASPECT_16_9, // 16:9のアスペクト比で描画
		FIXED_ASPECT_4_3,  // 4:3のアスペクト比で描画
		HD720,             // 1280x720で描画
		FHD1080,           // 1920x1080で描画
	};

	enum class EDITOR_VIEW_LAYOUT_MODE : uint8_t {
		SINGLE = 0,
		QUAD = 1,
	};

	class EditorRuntime {
	public:
		EditorRuntime(
			EditorWorld&          editorWorld,
			WindowManager&        windowManager,
			Render::RenderModule& renderModule,
			ImGuiLayer&           imGuiLayer
		);

		/// @brief この関数の後からImGuiを呼びだしてください。
		void BeginUI();

		/// @brief UIの構築と描画を行います。
		void BuildUi(float deltaTime);

		/// @brief プレゼントモードを切り替えます。
		void TogglePresentMode();

		/// @brief 現在のプレゼントモードを取得します。
		[[nodiscard]] EDITOR_PRESENT_MODE GetPresentMode() const;

		/// @brief シーンのレンダリング結果を設定します。
		void SetViewOutput(
			std::string_view               viewKey,
			const Render::SceneOutputView& output,
			Vec2                           size
		);
		void FillEditorRenderViews(Render::RenderFrameInputs& inputs);

		static constexpr std::string_view kViewScenePerspective =
			"editor.scene.perspective";
		static constexpr std::string_view kViewSceneTop = "editor.scene.top";
		static constexpr std::string_view kViewSceneFront = "editor.scene.front";
		static constexpr std::string_view kViewSceneRight = "editor.scene.right";
		static constexpr std::string_view kViewGuiPreview = "editor.gui.preview";

	private:
		struct ViewOutputCache {
			D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = {};
			uint64_t                    srvRevision = 0;
			uint32_t                    textureId = 0;
			Vec2                        size = Vec2::zero;
		};

		// Core
		/// @brief ImGuizmoの設定をロードします。
		static void LoadImGuizmoSettings();

		/// @brief 現在のシーンとエディタ状態に基づいて、シーンのレンダリング要求を構築します。
		/// @return シーンのレンダリング要求
		[[nodiscard]]
		Render::SceneViewRenderMode BuildSceneViewModeForSize(
			float width, float height, bool forceFit = false
		) const;
		/// @brief シーン階層を取得します。
		[[nodiscard]] Scene* GetOutlinerScene();

		/// @brief シーン階層を取得します（constバージョン）。
		[[nodiscard]] const Scene* GetOutlinerScene() const;

		// Chrome
		/// @brief メインメニューを描画します。
		void DrawMainMenu();
		/// @brief サイドバーを描画します。
		void DrawSideBar();
		/// @brief ステータスバーを描画します。
		void DrawStatusBar();

		// Viewport
		/// @brief ビューポートを描画します。
		void DrawViewport(float deltaTime);
		/// @brief ビューポートの上部にツールバーを描画します。
		void DrawViewportTopBar();
		/// @brief ビューポート上にギズモを描画します。
		/// @param sceneRequest 現在のシーンレンダリング要求
		/// @param imagePos ビューポートの描画開始位置
		/// @param drawWidth ビューポートの描画幅
		/// @param drawHeight ビューポートの描画高さ
		void DrawViewportGizmo(
			std::string_view                    viewKey,
			const Render::SceneViewRenderMode& sceneViewMode,
			const Vec2&                       imagePos,
			float                             drawWidth,
			float                             drawHeight
		);
		/// @brief ビューポート上のオーバーレイを描画します。
		void DrawViewportOverlay(float deltaTime);

		// Outliner
		/// @brief シーン階層ウィンドウを描画します。
		void DrawSceneOutliner();
		/// @brief インスペクタウィンドウを描画します。
		void DrawInspector();
		void DrawGuiEditor(float deltaTime);

		// Profiler
		/// @brief プロファイラウィンドウを描画します。
		void DrawProfilerWindow();

		// Utility
		/// @brief 現在選択されているエンティティを取得します。
		[[nodiscard]] Entity* GetSelectedEntity() const;

		/// @brief シーンを指定されたパスに保存します。
		/// @param path シーンを保存するパス
		/// @return 保存に成功した場合はtrue、失敗した場合はfalse
		bool SaveSceneAs(const std::string& path);

		[[nodiscard]] ViewportCameraBinding ResolveViewportBinding(
			std::string_view viewKey
		) const;

		EditorWorld&          mEditorWorld;
		WindowManager&        mWindowManager;
		Render::RenderModule& mRenderModule;
		ImGuiLayer&           mImGuiLayer;

		EntityId mSelectedEntityId = 0;

		EDITOR_PRESENT_MODE mPresentMode = EDITOR_PRESENT_MODE::VIEWPORT_PANEL;
		EDITOR_VIEWPORT_RENDER_MODE mViewportRenderMode =
			EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT;
		EDITOR_VIEW_LAYOUT_MODE mViewportLayoutMode =
			EDITOR_VIEW_LAYOUT_MODE::SINGLE;
		std::string mActiveViewportViewKey =
			std::string(kViewScenePerspective);
		Vec2 mViewportPosition = Vec2::zero;
		Vec2 mViewportSize = Vec2::zero;
		Vec2 mLastViewportSize = Vec2(kClientWidth, kClientHeight);
		float mViewportPanelWidth = kClientWidth;
		float mViewportPanelHeight = kClientHeight;
		bool mViewportSizeChangedThisFrame = false;
		EditorViewportCameraManager mCameraManager = {};
		std::unordered_map<std::string, ViewOutputCache> mViewOutputs;
		Vec2 mGuiPreviewResolution = Vec2(1920.0f, 1080.0f);
		bool mGuiEditorDockInitialized = false;

		std::unique_ptr<EditorNotification> mNotification;
		std::unique_ptr<Gui::UiDocumentManager> mGuiDocumentManager;
		std::shared_ptr<Gui::UiDocument> mGuiActiveDocument;
		std::unique_ptr<Gui::UiRoot> mGuiEditorRoot;
		std::unique_ptr<Gui::UiScreenStack> mGuiEditorScreenStack;
		std::unique_ptr<Gui::GuiEditorContext> mGuiEditorContext;
		std::vector<Gui::UiDrawCommand> mGuiPreviewDrawCommands;
		std::vector<Render::ScreenSpriteInput> mGuiPreviewSprites;
		bool mGuiPreviewTextWarningLogged = false;

		float mGridSnap        = 64.0f;
		float mAngleSnapDegree = 15.0f;

		float mCameraSpeedPopupTimer = 0.0f;

		bool mShowProfilerWindow = false;
		bool mShowGuiEditorWindow = false;
		bool mViewportLookActive = false;
	};
}

#endif
