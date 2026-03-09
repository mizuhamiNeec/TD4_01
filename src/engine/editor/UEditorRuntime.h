#pragma once
#ifdef _DEBUG

#include <cstdint>
#include <d3d12.h>
#include <string>

#include "EditorNotification.h"

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
		VIEWPORT_PANEL,        // ビューポートパネルに描画
		FULLSCREEN_SWAP_CHAIN, // フルスクリーンでスワップチェーンに直接描画
	};

	enum class EDITOR_VIEWPORT_RENDER_MODE : uint8_t {
		FIT_VIEWPORT,      // ビューポートに合わせて描画
		FIXED_ASPECT_16_9, // 16:9のアスペクト比で描画
		HD720,             // 1280x720で描画
		FHD1080,           // 1920x1080で描画
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
		void BuildUi(float deltaTime);

		/// @brief 現在のシーンレンダリング要求を取得します。
		/// @return 現在のシーンレンダリング要求
		[[nodiscard]] Render::SceneRenderRequest GetSceneRenderRequest() const;

		/// @brief プレゼントモードを切り替えます。
		void TogglePresentMode();

		/// @brief 現在のプレゼントモードを取得します。
		[[nodiscard]] EDITOR_PRESENT_MODE GetPresentMode() const;

		/// @brief シーンのレンダリング結果を設定します。
		void SetSceneOutput(
			const Render::SceneOutputView& sceneOutput,
			Vec2                           size
		);

	private:
		/// @brief ImGuizmoの設定をロードします。
		void LoadImGuizmoSettings();


		/// @brief 現在のシーンとエディタ状態に基づいて、シーンのレンダリング要求を構築します。
		/// @return シーンのレンダリング要求
		[[nodiscard]]
		Render::SceneRenderRequest BuildSceneRenderRequest() const;

		/// @brief シーン階層を取得します。
		[[nodiscard]] UScene* GetHierarchyScene();

		/// @brief シーン階層を取得します（constバージョン）。
		[[nodiscard]] const UScene* GetHierarchyScene() const;

		/// @brief ビューポート上にギズモを描画します。
		/// @param sceneRequest 現在のシーンレンダリング要求
		/// @param imagePos ビューポートの描画開始位置
		/// @param drawWidth ビューポートの描画幅
		/// @param drawHeight ビューポートの描画高さ
		void DrawViewportGizmo(
			const Render::SceneRenderRequest& sceneRequest,
			const Vec2&                       imagePos,
			float                             drawWidth,
			float                             drawHeight
		);

		/// @brief メインメニューを描画します。
		void DrawMainMenu();
		/// @brief ビューポートの上部にツールバーを描画します。
		void DrawViewportTopBar();
		/// @brief サイドバーを描画します。
		void DrawSideBar();
		/// @brief ステータスバーを描画します。
		void DrawStatusBar();
		/// @brief シーン階層ウィンドウを描画します。
		void DrawSceneHierarchy();
		/// @brief インスペクタウィンドウを描画します。
		void DrawInspector();
		/// @brief プロファイラウィンドウを描画します。
		void DrawProfilerWindow();
		/// @brief ビューポートを描画します。
		void DrawViewport(float deltaTime);
		/// @brief ビューポート上のオーバーレイを描画します。
		void DrawViewportOverlay(float deltaTime);

		/// @brief 現在選択されているエンティティを取得します。
		[[nodiscard]] Entity* GetSelectedEntity() const;

		/// @brief シーンを指定されたパスに保存します。
		/// @param path シーンを保存するパス
		/// @return 保存に成功した場合はtrue、失敗した場合はfalse
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

		std::unique_ptr<EditorNotification> mNotification;

		float mGridSnap        = 64.0f;
		float mAngleSnapDegree = 15.0f;

		float mCameraSpeedPopupTimer = 0.0f;

		bool mShowProfilerWindow = false;
		bool mViewportLookActive = false;
	};
}

#endif
