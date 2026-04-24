#ifdef _DEBUG
#include "LevelEditorTool.h"

#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>

#include "EditorNotification.h"
#include "sequence/SequenceCurvePanel.h"
#include "sequence/SequenceEditorController.h"
#include "sequence/SequenceTimelinePanel.h"

#include "core/io/json/JsonReader.h"
#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/Ui.h"
#include "engine/platform/Window.h"
#include "engine/platform/WindowManager.h"
#include "engine/render/Renderer.h"
#include "engine/scene/SceneSerializer.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/input/InputSystem.h"
#include "engine/world/EditorWorld.h"

#include "thirdparty/ImGuizmo/ImGuizmo.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] Vec2 ResolveSceneRenderExtentForInput(
			const Render::SceneViewRenderMode& request
		) {
			uint32_t width  = std::max(1u, request.viewportPanelWidth);
			uint32_t height = std::max(1u, request.viewportPanelHeight);

			switch (request.mode) {
				case Render::SCENE_RENDER_MODE::FIT_VIEWPORT: {
					break;
				}
				case Render::SCENE_RENDER_MODE::FIXED_ASPECT_16X9: {
					if (width * 9 > height * 16) {
						width = height * 16 / 9;
					} else {
						height = width * 9 / 16;
					}
					break;
				}
				case Render::SCENE_RENDER_MODE::FIXED_ASPECT_4X3: {
					if (width * 3 > height * 4) {
						width = height * 4 / 3;
					} else {
						height = width * 3 / 4;
					}
					break;
				}
				case Render::SCENE_RENDER_MODE::HD_720P: {
					width  = 1280;
					height = 720;
					break;
				}
				case Render::SCENE_RENDER_MODE::FHD_1080P: {
					width  = 1920;
					height = 1080;
					break;
				}
				default: {
					break;
				}
			}

			width  = std::clamp(width, 2u, 8192u);
			height = std::clamp(height, 2u, 8192u);
			if ((width & 1u) != 0u) {
				--width;
			}
			if ((height & 1u) != 0u) {
				--height;
			}

			return Vec2(static_cast<float>(width), static_cast<float>(height));
		}
	}

	LevelEditorTool::LevelEditorTool(
		WindowManager& windowManager,
		ImGuiLayer&    imGuiLayer
	) : mOwnedEditorWorld(std::make_unique<EditorWorld>()),
	    mEditorWorld(*mOwnedEditorWorld),
	    mWindowManager(windowManager),
	    mImGuiLayer(imGuiLayer) {
		mNotification = std::make_unique<EditorNotification>();
		mCameraManager.SetPaneBinding(
			kViewScenePerspective,
			{
				.kind = ViewportCameraBindingKind::EditorPerspective,
				.cameraEntityGuid = 0
			}
		);
	}

	LevelEditorTool::~LevelEditorTool() = default;

	void LevelEditorTool::Initialize(const EditorToolServices& services) {
		if (mInitialized) {
			return;
		}
		mEditorWorld.SetServices(
			{
				.console      = services.console,
				.inputSystem  = services.inputSystem,
				.profiler     = services.profiler,
				.assetManager = services.assetManager,
				.demoService  = services.demoService
			}
		);
		mEditorWorld.SetPlayWorldFactory(services.gameWorldFactory);
		mConsoleSystem = services.console;
		mInputSystem   = services.inputSystem;
		mEditorWorld.Initialize();
		mSequenceEditorController = std::make_unique<
			SequenceEditorController>();
		mSequenceTimelinePanel = std::make_unique<SequenceTimelinePanel>();
		mSequenceCurvePanel    = std::make_unique<SequenceCurvePanel>();
		mSequenceEditorController->Initialize(
			mEditorWorld.GetRuntimeSceneWorld(),
			services.assetManager
		);
		LoadImGuizmoSettings(mConsoleSystem);
		mInitialized = true;
	}

	void LevelEditorTool::Shutdown() {
		if (!mInitialized) {
			return;
		}
		if (mSequenceEditorController) {
			mSequenceEditorController->Shutdown();
		}
		mSequenceCurvePanel.reset();
		mSequenceTimelinePanel.reset();
		mSequenceEditorController.reset();
		mEditorWorld.Shutdown();
		mConsoleSystem = nullptr;
		mInputSystem   = nullptr;
		mViewOutputs.clear();
		mDockInitialized = false;
		mInitialized     = false;
	}

	void LevelEditorTool::BeginUI() {
		if (!mOpen) {
			return;
		}
		ImGuizmo::BeginFrame();
	}

	void LevelEditorTool::Tick(const EditorToolFrameContext& frameContext) {
		if (!mSequenceEditorController) {
			return;
		}
		mSequenceEditorController->SetWorld(
			mEditorWorld.GetRuntimeSceneWorld()
		);
		mSequenceEditorController->Tick(frameContext.unscaledDeltaTime);
	}

	void LevelEditorTool::BuildUi(const EditorToolFrameContext& frameContext) {
		if (!mOpen) {
			mDockInitialized = false;
			return;
		}

		constexpr ImGuiWindowFlags hostFlags =
			ImGuiWindowFlags_MenuBar |
			ImGuiWindowFlags_NoCollapse;
		if (!ImGui::Begin("Level Editor", &mOpen, hostFlags)) {
			ImGui::End();
			return;
		}

		DrawMainMenu();
		const ImGuiID dockSpaceId = ImGui::GetID("LevelEditorDockSpace");
		ImGui::DockSpace(
			dockSpaceId,
			ImVec2(0.0f, 0.0f),
			ImGuiDockNodeFlags_None
		);
		if (!mDockInitialized) {
			const ImVec2 dockNodeSize = ImGui::GetContentRegionAvail();
			if (dockNodeSize.x > 1.0f && dockNodeSize.y > 1.0f) {
				ImGui::DockBuilderRemoveNode(dockSpaceId);
				ImGui::DockBuilderAddNode(
					dockSpaceId,
					ImGuiDockNodeFlags_DockSpace
				);
				ImGui::DockBuilderSetNodeSize(
					dockSpaceId,
					ImVec2(
						std::max(1.0f, dockNodeSize.x),
						std::max(1.0f, dockNodeSize.y)
					)
				);
				ImGuiID dockMain = dockSpaceId;
				ImGuiID dockLeft = ImGui::DockBuilderSplitNode(
					dockMain,
					ImGuiDir_Left,
					0.22f,
					nullptr,
					&dockMain
				);
				ImGuiID dockRight = ImGui::DockBuilderSplitNode(
					dockMain,
					ImGuiDir_Right,
					0.28f,
					nullptr,
					&dockMain
				);
				ImGuiID dockBottom = ImGui::DockBuilderSplitNode(
					dockMain,
					ImGuiDir_Down,
					0.30f,
					nullptr,
					&dockMain
				);
				ImGui::DockBuilderDockWindow("Viewport", dockMain);
				ImGui::DockBuilderDockWindow("Outliner", dockLeft);
				ImGui::DockBuilderDockWindow("Inspector", dockRight);
				ImGui::DockBuilderDockWindow("Profiler", dockBottom);
				ImGui::DockBuilderDockWindow("Content Browser", dockBottom);
				ImGui::DockBuilderDockWindow("Sequence Timeline", dockBottom);
				ImGui::DockBuilderDockWindow("Sequence Curves", dockBottom);
				ImGui::DockBuilderFinish(dockSpaceId);
				mDockInitialized = true;
			}
		}

		const float deltaTime         = frameContext.unscaledDeltaTime;
		mViewportSizeChangedThisFrame = false;

		if (mPresentMode == EDITOR_PRESENT_MODE::VIEWPORT_PANEL) {
			ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
			DrawViewport(deltaTime);
			const auto input = mInputSystem;
			mEditorWorld.SetEditorCameraLookEnabled(mViewportLookActive);
			if (input) {
				if (const Window* window = mWindowManager.FindWindowById(
					mWindowManager.GetMainWindowId()
				)) {
					Vec2 virtualViewportSize = mViewportSize;
					if (const auto outputIt = mViewOutputs.find(
						mActiveViewportViewKey
					); outputIt != mViewOutputs.end()) {
						virtualViewportSize = Vec2(
							std::max(1.0f, outputIt->second.size.x),
							std::max(1.0f, outputIt->second.size.y)
						);
					}

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
						mViewportSize,
						virtualViewportSize
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

			ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
			DrawSceneOutliner();
			ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
			DrawInspector();
			ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
			DrawContentBrowser();
			ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
			DrawSequenceEditors();
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
			const Vec2 runtimeViewportSize = ResolveSceneRenderExtentForInput(
				sceneRequest
			);

			const auto input = mInputSystem;
			mEditorWorld.SetEditorCameraLookEnabled(
				input && input->IsHeld("ed_look")
			);
			if (input) {
				if (const Window* window = mWindowManager.FindWindowById(
					mWindowManager.GetMainWindowId()
				)) {
					// 投影座標と一致させるため、実レンダー解像度を入力ビューポートとして扱います。
					input->SetMouseClientViewportRect(
						Vec2::zero,
						runtimeViewportSize
					);
					input->SetMouseCursorLockClientPosition(
						window->GetHwnd(),
						{
							runtimeViewportSize.x * 0.5f,
							runtimeViewportSize.y * 0.5f
						}
					);
				} else {
					input->ClearMouseClientViewportRectOverride();
				}
			}
			mViewportLookActive = false;
		}

		// シーケンサーのモックアップ
		{
			ImGui::Begin("SequencerMock");

			// シーケンサーの上のバー
			{
				float barHeight = 48.0f;

				ImGui::BeginChild(
					"SequencerMock_TopBar",
					ImVec2(0.0f, barHeight),
					false,
					ImGuiWindowFlags_NoScrollbar
				);

				ImGui::SetCursorPos(ImVec2(16.0f, 0.0f));

				const float topBarContentHeight =
					ImGui::GetContentRegionAvail().y;

				Ui::Row(
					[&]() {
						Ui::CenteredY(
							[&]() {
								Ui::Row(
									[&]() {
										Ui::Text("00:01:23.45");
										Ui::Text("0/3000");
									},
									32.0f
								);
							},
							topBarContentHeight,
							ImGui::GetTextLineHeight()
						);

						const auto buttonSize = ImVec2(32.0f, 32.0f);
						auto       DrawCenteredIconButton =
							[&](const uint32_t icon, const bool emphasize) {
							Ui::CenteredY(
								[&]() {
									if (emphasize) {
										Ui::ScopedStyleColor scopedColor(
											ImGuiCol_Button,
											ImVec4(0.2f, 0.4f, 0.2f, 1.0f)
										);
										Ui::IconButton(
											icon,
											nullptr,
											buttonSize
										);
										return;
									}
									Ui::IconButton(
										icon,
										nullptr,
										buttonSize
									);
								},
								topBarContentHeight,
								buttonSize.y
							);
						};
						Ui::Row(
							[&]() {
								DrawCenteredIconButton(
									kIconSkipPrevious, false
								);
								DrawCenteredIconButton(kIconArrowBack2, false);
								DrawCenteredIconButton(kIconPlay, true);
								DrawCenteredIconButton(kIconSkipNext, false);
							},
							8.0f
						);
					}
				);

				ImGui::EndChild();
			}

			ImGui::End();
		}

		ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
		DrawProfilerWindow();

		if (mPresentMode == EDITOR_PRESENT_MODE::VIEWPORT_PANEL) {
			DrawStatusBar();
		}

		mNotification->Update(deltaTime);
		ImGui::End();
	}

	void LevelEditorTool::CollectRenderViews(
		Render::RenderFrameInputs& inputs
	) {
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
			const std::string_view      key,
			const float                 width,
			const float                 height,
			const ViewportCameraBinding binding,
			const bool                  exposeToUi,
			const bool                  presentToSwapChain
		) {
			Render::RenderViewInput view = sourceScene;
			view.viewKey                 = std::string(key);
			view.type                    = Render::RENDER_VIEW_TYPE::SCENE;
			view.sceneViewMode           = BuildSceneViewModeForSize(
				width,
				height,
				false
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
			const Render::RenderCameraInput* fallback = sourceScene.camera.
				valid ?
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

		inputs.views = std::move(composedViews);
	}

	void LevelEditorTool::EnumerateViewKeys(
		std::vector<std::string>& outViewKeys
	) const {
		outViewKeys.emplace_back(kViewScenePerspective);
	}

	void LevelEditorTool::SetViewOutput(
		const std::string_view         viewKey,
		const Render::SceneOutputView& output,
		const Vec2                     size
	) {
		ViewOutputCache cache;
		cache.textureId                    = output.textureId;
		cache.srvCpu                       = output.srvCpu;
		cache.srvRevision                  = output.srvRevision;
		cache.size                         = size;
		mViewOutputs[std::string(viewKey)] = cache;
	}

	std::string_view LevelEditorTool::GetToolId() const {
		return "tool.level";
	}

	std::string_view LevelEditorTool::GetDisplayName() const {
		return "Level Editor";
	}

	World* LevelEditorTool::GetRuntimeWorld() {
		return &mEditorWorld;
	}

	bool LevelEditorTool::IsOpen() const {
		return mOpen;
	}

	void LevelEditorTool::SetOpen(const bool open) {
		if (mOpen != open) {
			mViewOutputs.clear();
		}
		mOpen = open;
		if (!mOpen) {
			mDockInitialized    = false;
			mViewportLookActive = false;
		}
	}

	void LevelEditorTool::TogglePresentMode() {
		mPresentMode = mPresentMode == EDITOR_PRESENT_MODE::VIEWPORT_PANEL ?
			               EDITOR_PRESENT_MODE::FULLSCREEN_SWAP_CHAIN :
			               EDITOR_PRESENT_MODE::VIEWPORT_PANEL;
	}

	EDITOR_PRESENT_MODE LevelEditorTool::GetPresentMode() const {
		return mPresentMode;
	}

	bool LevelEditorTool::IsPlaying() const {
		return mEditorWorld.IsPlaying();
	}

	void LevelEditorTool::StartPlayInEditor() const {
		mEditorWorld.StartPlayInEditor();
	}

	void LevelEditorTool::StopPlayInEditor() const {
		mEditorWorld.StopPlayInEditor();
	}

	bool LevelEditorTool::IsProfilerWindowOpen() const {
		return mShowProfilerWindow;
	}

	void LevelEditorTool::SetProfilerWindowOpen(const bool open) {
		mShowProfilerWindow = open;
	}

	void LevelEditorTool::LoadImGuizmoSettings(ConsoleSystem* console) {
		if (!console) {
			return;
		}

		auto guizmoConfig = console->GetConVarAs<ConVar<std::string>>(
			"im_guizmoconfigpath"
		);

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

	Render::SceneViewRenderMode LevelEditorTool::BuildSceneViewModeForSize(
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
			forceFit ?
				EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT :
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

	Scene* LevelEditorTool::GetOutlinerScene() {
		return mEditorWorld.IsPlaying() ?
			       mEditorWorld.GetActiveScene() :
			       mEditorWorld.GetEditableScene();
	}

	const Scene* LevelEditorTool::GetOutlinerScene() const {
		return mEditorWorld.IsPlaying() ?
			       mEditorWorld.GetActiveScene() :
			       mEditorWorld.GetEditableScene();
	}

	Entity* LevelEditorTool::GetSelectedEntity() const {
		const Scene* scene = GetOutlinerScene();
		if (!scene || mSelectedEntityId == 0) {
			return nullptr;
		}
		return const_cast<Scene*>(scene)->FindEntity(mSelectedEntityId);
	}

	bool LevelEditorTool::SaveSceneAs(const std::string& path) {
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

	bool LevelEditorTool::LoadSceneFromPath(const std::string& path) {
		const std::string normalizedPath = StrUtil::NormalizePath(
			StrUtil::TrimSpaces(path)
		);
		if (normalizedPath.empty()) {
			return false;
		}

		// 再生中はプレイワールドを停止し、編集ワールドへロード
		if (mEditorWorld.IsPlaying()) {
			mEditorWorld.StopPlayInEditor();
		}

		if (!mEditorWorld.LoadSceneFromFile(normalizedPath.c_str())) {
			Warning(
				"LevelEditorTool",
				"Failed to load scene: {}",
				normalizedPath
			);
			if (mNotification) {
				mNotification->PushNotification(
					"Scene Load Failed",
					normalizedPath,
					NOTIFY_TYPE::ERR
				);
			}
			return false;
		}

		mSelectedEntityId = 0;
		Msg("LevelEditorTool", "Scene loaded: {}", normalizedPath);
		if (mNotification) {
			mNotification->PushNotification(
				"Scene Loaded",
				normalizedPath,
				NOTIFY_TYPE::INFO
			);
		}

		return true;
	}

	ViewportCameraBinding LevelEditorTool::ResolveViewportBinding(
		const std::string_view viewKey
	) const {
		return mCameraManager.GetPaneBinding(viewKey);
	}
}

#endif
