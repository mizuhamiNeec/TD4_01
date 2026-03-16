#ifdef _DEBUG
#include "EditorRuntime.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_internal.h>

#include "EditorNotification.h"

#include "core/assets/AssetManager.h"
#include "core/json/JsonReader.h"
#include "core/string/StrUtil.h"

#include "engine/gui/UiDocumentManager.h"
#include "engine/gui/UiDrawCommand.h"
#include "engine/gui/UiRoot.h"
#include "engine/gui/UiScreenStack.h"
#include "engine/gui/editor/GuiEditor.h"
#include "engine/platform/Window.h"
#include "engine/platform/WindowManager.h"
#include "engine/render/RenderModule.h"
#include "engine/scene/SceneSerializer.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/EditorWorld.h"

#include "thirdparty/ImGuizmo/ImGuizmo.h"

namespace Unnamed {
	namespace {
		Vec4 ToVec4(const Gui::Color& color) {
			return Vec4(color.r, color.g, color.b, color.a);
		}

		Render::ScreenSpriteInput BuildScreenSprite(
			const Gui::UiDrawCommandRect& rect,
			const int32_t                 sortKey
		) {
			Render::ScreenSpriteInput sprite = {};
			sprite.texture.source = Render::SPRITE_TEXTURE_SOURCE::ASSET;
			sprite.texture.textureAssetId = kInvalidAssetID;
			sprite.positionPx = Vec2(rect.rect.x, rect.rect.y);
			sprite.sizePx = Vec2(rect.rect.width, rect.rect.height);
			sprite.anchor = Vec2(0.0f, 0.0f);
			sprite.rotationRad = 0.0f;
			sprite.color = ToVec4(rect.fillColor);
			sprite.sortKey = sortKey;
			return sprite;
		}
	}

	EditorRuntime::EditorRuntime(
		EditorWorld&          editorWorld,
		WindowManager&        windowManager,
		Render::RenderModule& renderModule,
		ImGuiLayer&           imGuiLayer
	) : mEditorWorld(editorWorld),
	    mWindowManager(windowManager),
	    mRenderModule(renderModule),
	    mImGuiLayer(imGuiLayer) {
		LoadImGuizmoSettings();
		mNotification       = std::make_unique<EditorNotification>();
		mGuiDocumentManager = std::make_unique<Gui::UiDocumentManager>(
			ServiceLocator::Get<AssetManager>()
		);
		mGuiEditorRoot        = std::make_unique<Gui::UiRoot>();
		mGuiEditorScreenStack = std::make_unique<Gui::UiScreenStack>(
			mGuiEditorRoot.get()
		);
		mGuiEditorContext = std::make_unique<Gui::GuiEditorContext>();

		mCameraManager.SetPaneBinding(
			kViewScenePerspective,
			{
				.kind = ViewportCameraBindingKind::EditorPerspective,
				.cameraEntityGuid = 0
			}
		);
		mCameraManager.SetPaneBinding(
			kViewSceneTop,
			{
				.kind = ViewportCameraBindingKind::EditorOrthoTop,
				.cameraEntityGuid = 0
			}
		);
		mCameraManager.SetPaneBinding(
			kViewSceneFront,
			{
				.kind = ViewportCameraBindingKind::EditorOrthoFront,
				.cameraEntityGuid = 0
			}
		);
		mCameraManager.SetPaneBinding(
			kViewSceneRight,
			{
				.kind = ViewportCameraBindingKind::EditorOrthoRight,
				.cameraEntityGuid = 0
			}
		);
	}

	void EditorRuntime::BeginUI() {
		ImGuizmo::BeginFrame();

		DrawMainMenu();
		DrawSideBar();
		DrawStatusBar();

		// DockSpaceの開始
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(
				ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)
			);

			constexpr ImGuiDockNodeFlags dockSpaceFlags =
				ImGuiDockNodeFlags_None;
			constexpr ImGuiWindowFlags windowFlags =
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoNavFocus |
				ImGuiWindowFlags_NoBackground;

			ImGui::Begin("DockSpace", nullptr, windowFlags);

			ImGui::PopStyleVar(3);

			const ImGuiIO& io = ImGui::GetIO();
			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
				const ImGuiID dockSpaceId = ImGui::GetID("DockSpace");
				ImGui::DockSpace(
					dockSpaceId, ImVec2(0.0f, 0.0f), dockSpaceFlags
				);
			}

			ImGui::End();
		}

		ImGui::ShowDemoWindow();
	}

	void EditorRuntime::BuildUi(const float deltaTime) {
		mViewportSizeChangedThisFrame = false;

		if (mPresentMode == EDITOR_PRESENT_MODE::VIEWPORT_PANEL) {
			DrawViewport(deltaTime);
			const auto input = ServiceLocator::Get<InputSystem>();
			mEditorWorld.SetEditorCameraLookEnabled(mViewportLookActive);
			if (input) {
				if (const Window* window = mWindowManager.FindWindowById(
					mWindowManager.GetMainWindowId()
				)) {
					POINT viewportClientTopLeft = {
						static_cast<LONG>(mViewportPosition.x),
						static_cast<LONG>(mViewportPosition.y)
					};
					ScreenToClient(window->GetHwnd(), &viewportClientTopLeft);
					input->SetMouseClientViewportRect(
						Vec2(
							static_cast<float>(viewportClientTopLeft.x),
							static_cast<float>(viewportClientTopLeft.y)
						),
						mViewportSize
					);

					if (mViewportLookActive) {
						POINT clientPoint = {
							static_cast<LONG>(
								mViewportPosition.x + mViewportSize.x * 0.5f),
							static_cast<LONG>(
								mViewportPosition.y + mViewportSize.y * 0.5f)
						};
						ScreenToClient(window->GetHwnd(), &clientPoint);
						input->SetMouseCursorLockClientPosition(
							window->GetHwnd(),
							Vec2(
								static_cast<float>(clientPoint.x),
								static_cast<float>(clientPoint.y)
							)
						);
					} else {
						input->ClearMouseCursorLockAnchor();
					}
				} else {
					input->ClearMouseClientViewportRectOverride();
					input->ClearMouseCursorLockAnchor();
				}
			}

			DrawSceneOutliner();
			DrawInspector();
		} else {
			float width  = mViewportPanelWidth;
			float height = mViewportPanelHeight;
			if (const Window* window = mWindowManager.FindWindowById(
				mWindowManager.GetMainWindowId()
			)) {
				const WindowDesc desc = window->GetDesc();
				width = static_cast<float>(std::max(1, desc.width));
				height = static_cast<float>(std::max(1, desc.height));
			}
			const Render::SceneViewRenderMode sceneRequest =
				BuildSceneViewModeForSize(width, height);

			const auto input = ServiceLocator::Get<InputSystem>();
			mEditorWorld.SetEditorCameraLookEnabled(
				input && input->IsHeld("ed_look")
			);
			if (input) {
				if (const Window* window = mWindowManager.FindWindowById(
					mWindowManager.GetMainWindowId()
				)) {
					input->SetMouseClientViewportRect(
						Vec2::zero,
						Vec2(
							width,
							height
						)
					);
					input->SetMouseCursorLockClientPosition(
						window->GetHwnd(),
						{
							static_cast<float>(sceneRequest.viewportPanelWidth)
							*
							0.5f,
							static_cast<float>(sceneRequest.viewportPanelHeight)
							*
							0.5f
						}
					);
				} else {
					input->ClearMouseClientViewportRectOverride();
				}
			}
			mViewportLookActive = false;
		}

		DrawGuiEditor(deltaTime);
		DrawProfilerWindow();

		mNotification->Update(deltaTime);
	}

	void EditorRuntime::TogglePresentMode() {
		mPresentMode = mPresentMode == EDITOR_PRESENT_MODE::VIEWPORT_PANEL ?
			               EDITOR_PRESENT_MODE::FULLSCREEN_SWAP_CHAIN :
			               EDITOR_PRESENT_MODE::VIEWPORT_PANEL;
	}

	EDITOR_PRESENT_MODE EditorRuntime::GetPresentMode() const {
		return mPresentMode;
	}

	void EditorRuntime::SetViewOutput(
		const std::string_view         viewKey,
		const Render::SceneOutputView& output,
		const Vec2                     size
	) {
		ViewOutputCache cache              = {};
		cache.textureId                    = output.textureId;
		cache.srvCpu                       = output.srvCpu;
		cache.srvRevision                  = output.srvRevision;
		cache.size                         = size;
		mViewOutputs[std::string(viewKey)] = cache;
	}

	void EditorRuntime::FillEditorRenderViews(Render::RenderFrameInputs& inputs) {
		Render::RenderViewInput              sourceScene = {};
		bool                                 hasScene    = false;
		std::vector<Render::RenderViewInput> preservedViews;
		preservedViews.reserve(inputs.views.size());
		for (const auto& view : inputs.views) {
			if (!hasScene && view.type == Render::RENDER_VIEW_TYPE::SCENE) {
				sourceScene = view;
				hasScene    = true;
				continue;
			}
			preservedViews.emplace_back(view);
		}

		if (!hasScene) {
			sourceScene.viewKey         = std::string(kViewScenePerspective);
			sourceScene.type            = Render::RENDER_VIEW_TYPE::SCENE;
			sourceScene.output.sizeMode =
				Render::RENDER_VIEW_SIZE_MODE::MATCH_BACK_BUFFER;
		}

		auto buildSceneView = [this, &sourceScene](
			const std::string_view key,
			const float            width,
			const float            height,
			const ViewportCameraBinding binding,
			const bool             exposeToUi,
			const bool             presentToSwapChain
		) {
			Render::RenderViewInput view = sourceScene;
			view.viewKey = std::string(key);
			view.type = Render::RENDER_VIEW_TYPE::SCENE;
			view.sceneViewMode = BuildSceneViewModeForSize(
				width,
				height,
				mViewportLayoutMode == EDITOR_VIEW_LAYOUT_MODE::QUAD
			);
			view.output.sizeMode = Render::RENDER_VIEW_SIZE_MODE::FIXED;
			view.output.width = view.sceneViewMode.viewportPanelWidth;
			view.output.height = view.sceneViewMode.viewportPanelHeight;
			view.output.presentToSwapChain = presentToSwapChain;
			view.output.clearSwapChainWhenNotPresenting = !presentToSwapChain;
			view.output.exposeToUi = exposeToUi;

			mCameraManager.SyncGameplayCameraAspect(
				mEditorWorld, view.sceneViewMode, binding
			);
			const Render::RenderCameraInput* fallback = sourceScene.camera.valid ?
				                                            &sourceScene.camera :
				                                            nullptr;
			const EditorViewportCameraManager::ResolvedCamera resolved =
				mCameraManager.ResolveViewCamera(
					mEditorWorld,
					key,
					view.sceneViewMode,
					binding,
					fallback
				);
			if (resolved.input.valid) {
				view.camera = resolved.input;
			}

			return view;
		};

		std::vector<Render::RenderViewInput> composedViews;
		composedViews.reserve(8 + preservedViews.size());
		composedViews.insert(
			composedViews.end(), preservedViews.begin(), preservedViews.end()
		);

		if (mViewportLayoutMode == EDITOR_VIEW_LAYOUT_MODE::QUAD) {
			const float paneW = std::max(1.0f, mViewportPanelWidth * 0.5f);
			const float paneH = std::max(1.0f, mViewportPanelHeight * 0.5f);
			const bool  fullscreenPresent =
				mPresentMode == EDITOR_PRESENT_MODE::FULLSCREEN_SWAP_CHAIN;
			const std::string presentKey = fullscreenPresent ?
				                               (mActiveViewportViewKey.empty() ?
					                                std::string(
						                                kViewScenePerspective
					                                ) :
					                                mActiveViewportViewKey) :
				                               std::string();
			composedViews.emplace_back(
				buildSceneView(
					kViewScenePerspective,
					paneW,
					paneH,
					ResolveViewportBinding(kViewScenePerspective),
					true,
					fullscreenPresent &&
					presentKey == kViewScenePerspective
				)
			);
			composedViews.emplace_back(
				buildSceneView(
					kViewSceneTop,
					paneW,
					paneH,
					ResolveViewportBinding(kViewSceneTop),
					true,
					fullscreenPresent && presentKey == kViewSceneTop
				)
			);
			composedViews.emplace_back(
				buildSceneView(
					kViewSceneFront,
					paneW,
					paneH,
					ResolveViewportBinding(kViewSceneFront),
					true,
					fullscreenPresent && presentKey == kViewSceneFront
				)
			);
			composedViews.emplace_back(
				buildSceneView(
					kViewSceneRight,
					paneW,
					paneH,
					ResolveViewportBinding(kViewSceneRight),
					true,
					fullscreenPresent && presentKey == kViewSceneRight
				)
			);
		} else {
			composedViews.emplace_back(
				buildSceneView(
					kViewScenePerspective,
					std::max(1.0f, mViewportPanelWidth),
					std::max(1.0f, mViewportPanelHeight),
					ResolveViewportBinding(kViewScenePerspective),
					true,
					mPresentMode == EDITOR_PRESENT_MODE::FULLSCREEN_SWAP_CHAIN
				)
			);
		}

		if (mShowGuiEditorWindow) {
			Render::RenderViewInput previewView = {};
			previewView.viewKey = std::string(kViewGuiPreview);
			previewView.type = Render::RENDER_VIEW_TYPE::SPRITE_ONLY;
			previewView.output.sizeMode = Render::RENDER_VIEW_SIZE_MODE::FIXED;
			previewView.output.width = static_cast<uint32_t>(std::max(
				1.0f, mGuiPreviewResolution.x
			));
			previewView.output.height = static_cast<uint32_t>(std::max(
				1.0f, mGuiPreviewResolution.y
			));
			previewView.output.exposeToUi = true;
			previewView.screenSprites     = mGuiPreviewSprites;
			composedViews.emplace_back(std::move(previewView));
		}

		inputs.views = std::move(composedViews);
	}

	void EditorRuntime::LoadImGuizmoSettings() {
		auto guizmoConfig = ServiceLocator::Get<ConsoleSystem>()->GetConVarAs<
			ConVar<std::string>>("im_guizmoconfigpath");

		auto Vec4ToImVec4 = [](const Vec4& vec) {
			return ImVec4(vec.x, vec.y, vec.z, vec.w);
		};

		if (guizmoConfig) {
			JsonReader reader(guizmoConfig->GetValue());
			auto&      style  = ImGuizmo::GetStyle();
			auto&      colors = style.Colors;

			// Style
			{
				const auto rs                  = reader["style"];
				style.TranslationLineThickness =
					rs["translationLineThickness"].GetFloat();
				style.TranslationLineArrowSize =
					rs["translationLineArrowSize"].GetFloat();
				style.RotationLineThickness =
					rs["rotationLineThickness"].GetFloat();
				style.RotationOuterLineThickness =
					rs["rotationOuterLineThickness"].GetFloat();
				style.ScaleLineThickness =
					rs["scaleLineThickness"].GetFloat();
				style.ScaleLineCircleSize =
					rs["scaleLineCircleSize"].GetFloat();
				style.HatchedAxisLineThickness =
					rs["hatchedAxisLineThickness"].GetFloat();
				style.CenterCircleSize =
					rs["centerCircleSize"].GetFloat();
			}

			// Color
			{
				using namespace ImGuizmo;
				auto       rc = reader["color"];
				const auto dx = rc["directionX"].GetArray();
				colors[DIRECTION_X] = Vec4ToImVec4(dx.GetVec4());
				const auto dy = rc["directionY"].GetArray();
				colors[DIRECTION_Y] = Vec4ToImVec4(dy.GetVec4());
				const auto dz = rc["directionZ"].GetArray();
				colors[DIRECTION_Z] = Vec4ToImVec4(dz.GetVec4());
				const auto px = rc["planeX"].GetArray();
				colors[PLANE_X] = Vec4ToImVec4(px.GetVec4());
				const auto py = rc["planeY"].GetArray();
				colors[PLANE_Y] = Vec4ToImVec4(py.GetVec4());
				const auto pz = rc["planeZ"].GetArray();
				colors[PLANE_Z] = Vec4ToImVec4(pz.GetVec4());
				const auto sel = rc["selection"].GetArray();
				colors[SELECTION] = Vec4ToImVec4(sel.GetVec4());
				const auto inact = rc["inactive"].GetArray();
				colors[INACTIVE] = Vec4ToImVec4(inact.GetVec4());
				const auto tline = rc["translationLine"].GetArray();
				colors[TRANSLATION_LINE] = Vec4ToImVec4(tline.GetVec4());
				const auto sline = rc["scaleLine"].GetArray();
				colors[SCALE_LINE] = Vec4ToImVec4(sline.GetVec4());
				const auto rborder = rc["rotationUsingBorder"].GetArray();
				colors[ROTATION_USING_BORDER] = Vec4ToImVec4(rborder.GetVec4());
				const auto rfill = rc["rotationUsingFill"].GetArray();
				colors[ROTATION_USING_FILL] = Vec4ToImVec4(rfill.GetVec4());
				const auto hatch = rc["hatchedAxisLines"].GetArray();
				colors[HATCHED_AXIS_LINES] = Vec4ToImVec4(hatch.GetVec4());
				const auto text = rc["text"].GetArray();
				colors[TEXT] = Vec4ToImVec4(text.GetVec4());
				const auto textShadow = rc["textShadow"].GetArray();
				colors[TEXT_SHADOW] = Vec4ToImVec4(textShadow.GetVec4());
			}

			// どっちが正方向かわからんくなるので禁止
			ImGuizmo::AllowAxisFlip(false);
		}
	}

	Render::SceneViewRenderMode EditorRuntime::BuildSceneViewModeForSize(
		const float width, const float height, const bool forceFit
	) const {
		Render::SceneViewRenderMode sceneRequest = {};
		sceneRequest.preferRealtimeResize = true;
		sceneRequest.viewportPanelWidth = static_cast<uint32_t>(std::max(
			1.0f, width
		));
		sceneRequest.viewportPanelHeight = static_cast<uint32_t>(std::max(
			1.0f, height
		));

		const EDITOR_VIEWPORT_RENDER_MODE renderMode =
			forceFit ? EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT :
			           mViewportRenderMode;
		switch (renderMode) {
			case EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::FIT_VIEWPORT;
				break;
			case EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_16_9
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9;
				break;
			case EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_4_3
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::FIXED_ASPECT_4X3;
				break;
			case EDITOR_VIEWPORT_RENDER_MODE::HD720
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::HD_720P;
				break;
			case EDITOR_VIEWPORT_RENDER_MODE::FHD1080
			: sceneRequest.mode = Render::SCENE_RENDER_MODE::FHD_1080P;
				break;
			default: break;
		}

		const bool dynamicMode = sceneRequest.mode ==
		                         Render::SCENE_RENDER_MODE::FIT_VIEWPORT ||
		                         sceneRequest.mode ==
		                         Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9 ||
		                         sceneRequest.mode ==
		                         Render::SCENE_RENDER_MODE::FIXED_ASPECT_4X3;
		if (dynamicMode) {
			sceneRequest.viewportPanelWidth = std::max(
				8u, sceneRequest.viewportPanelWidth / 8u * 8u
			);
			sceneRequest.viewportPanelHeight = std::max(
				8u, sceneRequest.viewportPanelHeight / 8u * 8u
			);
		}

		return sceneRequest;
	}

	Scene* EditorRuntime::GetOutlinerScene() {
		return mEditorWorld.IsPlaying() ?
			       mEditorWorld.GetActiveScene() :
			       mEditorWorld.GetEditableScene();
	}

	const Scene* EditorRuntime::GetOutlinerScene() const {
		return mEditorWorld.IsPlaying() ?
			       mEditorWorld.GetActiveScene() :
			       mEditorWorld.GetEditableScene();
	}

	void EditorRuntime::DrawGuiEditor(const float deltaTime) {
		if (!mShowGuiEditorWindow) {
			mGuiEditorDockInitialized = false;
			return;
		}
		if (
			!mGuiDocumentManager ||
			!mGuiEditorRoot ||
			!mGuiEditorScreenStack ||
			!mGuiEditorContext
		) {
			return;
		}

		(void)mGuiDocumentManager->UpdateTrackedDocuments();
		if (!mGuiEditorContext->activeDocumentPath.empty()) {
			if (auto latest = mGuiDocumentManager->GetDocument(
				mGuiEditorContext->activeDocumentPath
			); latest && latest != mGuiActiveDocument) {
				mGuiActiveDocument                = latest;
				mGuiEditorContext->selectedWidget = nullptr;
				mGuiEditorScreenStack->Clear();
				mGuiEditorScreenStack->PushScreen(
					std::make_shared<Gui::UiScreen>(latest)
				);
			}
		}
		mGuiEditorContext->documentChanged = false;

		ImGuiWindowFlags hostFlags =
			ImGuiWindowFlags_MenuBar |
			ImGuiWindowFlags_NoCollapse;
		if (!ImGui::Begin("GUI Editor", &mShowGuiEditorWindow, hostFlags)) {
			ImGui::End();
			return;
		}
		const ImGuiID dockSpaceId = ImGui::GetID("GUIEditorDockSpace");
		ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
		if (!mGuiEditorDockInitialized) {
			const ImVec2 dockNodeSize = ImGui::GetContentRegionAvail();
			if (dockNodeSize.x <= 1.0f || dockNodeSize.y <= 1.0f) {
				ImGui::End();
				return;
			}
			ImGui::DockBuilderRemoveNode(dockSpaceId);
			ImGui::DockBuilderAddNode(
				dockSpaceId, ImGuiDockNodeFlags_DockSpace
			);
			ImGui::DockBuilderSetNodeSize(
				dockSpaceId,
				ImVec2(std::max(1.0f, dockNodeSize.x), std::max(1.0f, dockNodeSize.y))
			);
			ImGuiID dockMain   = dockSpaceId;
			ImGuiID dockLeft   = ImGui::DockBuilderSplitNode(
				dockMain, ImGuiDir_Left, 0.25f, nullptr, &dockMain
			);
			ImGuiID dockRight  = ImGui::DockBuilderSplitNode(
				dockMain, ImGuiDir_Right, 0.30f, nullptr, &dockMain
			);
			ImGuiID dockBottom = ImGui::DockBuilderSplitNode(
				dockMain, ImGuiDir_Down, 0.35f, nullptr, &dockMain
			);
			ImGuiID dockLeftBottom = ImGui::DockBuilderSplitNode(
				dockLeft, ImGuiDir_Down, 0.45f, nullptr, &dockLeft
			);
			ImGui::DockBuilderDockWindow("UI Document", dockLeft);
			ImGui::DockBuilderDockWindow("Ui Outliner", dockLeft);
			ImGui::DockBuilderDockWindow("Ui Palette", dockLeftBottom);
			ImGui::DockBuilderDockWindow("Ui Inspector", dockRight);
			ImGui::DockBuilderDockWindow("Ui Preview", dockBottom);
			ImGui::DockBuilderFinish(dockSpaceId);
			mGuiEditorDockInitialized = true;
		}
		ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
		Gui::DrawUiEditorMenu(
			*mGuiDocumentManager,
			mGuiActiveDocument,
			mGuiEditorScreenStack.get(),
			*mGuiEditorContext
		);
		ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
		Gui::DrawUiHierarchyWindow(*mGuiEditorRoot, *mGuiEditorContext);
		ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
		Gui::DrawUiPaletteWindow(*mGuiEditorRoot, *mGuiEditorContext);
		ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
		Gui::DrawUiInspectorWindow(*mGuiEditorContext);

		if (mGuiEditorContext->documentChanged) {
			const std::string trackingPath =
				!mGuiEditorContext->activeDocumentPath.empty() ?
					mGuiEditorContext->activeDocumentPath :
					StrUtil::NormalizePath(
						mGuiEditorContext->pathBuffer.data()
					);
			mGuiDocumentManager->MarkDirty(trackingPath, true);
		}

		mGuiEditorRoot->SetRootSize(
			std::max(1.0f, mGuiPreviewResolution.x),
			std::max(1.0f, mGuiPreviewResolution.y)
		);
		mGuiEditorScreenStack->Tick(deltaTime);
		mGuiEditorRoot->Tick(deltaTime);
		mGuiEditorRoot->UpdateLayout();

		Render::SceneOutputView previewOutput = {};
		Vec2                    previewSize   = Vec2::zero;
		if (const auto it = mViewOutputs.find(std::string(kViewGuiPreview));
			it != mViewOutputs.end()) {
			previewOutput.textureId   = it->second.textureId;
			previewOutput.srvCpu      = it->second.srvCpu;
			previewOutput.srvRevision = it->second.srvRevision;
			previewSize               = it->second.size;
		}

		ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
		Gui::DrawUiPreviewWindow(
			mGuiEditorRoot.get(),
			previewOutput,
			previewSize,
			mImGuiLayer,
			*mGuiEditorContext,
			mGuiPreviewResolution
		);
		ImGui::End();

		mGuiPreviewDrawCommands.clear();
		mGuiEditorRoot->BuildDrawCommands(mGuiPreviewDrawCommands);

		mGuiPreviewSprites.clear();
		mGuiPreviewSprites.reserve(mGuiPreviewDrawCommands.size());
		for (size_t i = 0; i < mGuiPreviewDrawCommands.size(); ++i) {
			const auto& draw = mGuiPreviewDrawCommands[i];
			if (draw.type == Gui::UiDrawCommandType::RECT) {
				mGuiPreviewSprites.emplace_back(
					BuildScreenSprite(draw.rect, static_cast<int32_t>(i))
				);
				continue;
			}

			if (!mGuiPreviewTextWarningLogged) {
				Warning(
					kChannelNone,
					"UiDrawCommand TEXT is not supported in GUI preview pass yet (Phase2)."
				);
				mGuiPreviewTextWarningLogged = true;
			}
		}
	}

	ViewportCameraBinding EditorRuntime::ResolveViewportBinding(
		const std::string_view viewKey
	) const {
		return mCameraManager.GetPaneBinding(viewKey);
	}

	Entity* EditorRuntime::GetSelectedEntity() const {
		const Scene* scene = GetOutlinerScene();
		if (!scene || mSelectedEntityId == 0) {
			return nullptr;
		}
		return const_cast<Scene*>(scene)->FindEntity(mSelectedEntityId);
	}

	bool EditorRuntime::SaveSceneAs(const std::string& path) {
		const Scene* scene = mEditorWorld.GetEditableScene();
		if (!scene) {
			return false;
		}
		if (!SceneSerializer::SaveToFile(*scene, path)) {
			return false;
		}
		mEditorWorld.SetLoadedScenePath(path);
		return true;
	}
}

#endif
