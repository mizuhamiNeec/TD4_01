#ifdef _DEBUG
#include "LevelEditorTool.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <imgui.h>

#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/ui/ImGuiLayer.h"
#include "engine/unnamed/framework/components/CameraComponent.h"
#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/framework/components/editor/EditorCameraComponent.h"
#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/world/EditorWorld.h"

#include "thirdparty/ImGuizmo/ImGuizmo.h"

namespace Unnamed {
	void LevelEditorTool::DrawViewportGizmo(
		const std::string_view                 viewKey,
		const Render::SceneViewRenderMode& sceneViewMode,
		const Vec2&                        imagePos,
		const float                        drawWidth,
		const float                        drawHeight
	) {
		Entity* entity = GetSelectedEntity();
		if (!entity) {
			return;
		}

		auto* transform = entity->GetComponent<TransformComponent>();
		if (!transform) {
			return;
		}
		Mat4 world = transform->WorldMat();

		const EditorViewportCameraManager::ResolvedCamera camera = mCameraManager.
			ResolveViewCamera(
				mEditorWorld,
				viewKey,
				sceneViewMode,
				ResolveViewportBinding(viewKey),
				nullptr
			);
		if (!camera.input.valid) {
			return;
		}
		const Mat4 view = camera.input.view;
		const Mat4 proj = camera.input.proj;

		static ImGuizmo::OPERATION sOperation = ImGuizmo::TRANSLATE;
		static ImGuizmo::MODE      sMode      = ImGuizmo::WORLD;

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
			if (ImGui::IsKeyPressed(ImGuiKey_W)) {
				sOperation = ImGuizmo::TRANSLATE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_E)) {
				sOperation = ImGuizmo::ROTATE;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_R)) {
				sOperation = ImGuizmo::SCALE;
			}
		}

		ImGuizmo::SetOrthographic(camera.isOrthographic);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(imagePos.x, imagePos.y, drawWidth, drawHeight);

		const bool  useSnap = sOperation != ImGuizmo::SCALE;
		const float snap[3] = {
			sOperation == ImGuizmo::ROTATE ? mAngleSnapDegree : mGridSnap,
			sOperation == ImGuizmo::ROTATE ? mAngleSnapDegree : mGridSnap,
			sOperation == ImGuizmo::ROTATE ? mAngleSnapDegree : mGridSnap
		};

		if (
			ImGuizmo::Manipulate(
				*view.m,
				*proj.m,
				sOperation,
				sMode,
				*world.m,
				nullptr,
				useSnap ? snap : nullptr
			)
		) {
			const Mat4 editedWorld = world;
			Mat4       localMat    = editedWorld;

			const Vec3 localScale  = localMat.GetScale();
			const Mat4 rotationMat = localMat;

			transform->SetPosition(localMat.GetTranslate());
			transform->SetRotation(rotationMat.ToQuaternion());
			transform->SetScale(localScale);
		}
	}

	void LevelEditorTool::DrawViewportTopBar() {
		const float toolbarHeight            = ImGui::GetFontSize() * 2.0f;
		const float toolbarHeightWithPadding =
			toolbarHeight +
			ImGui::GetStyle().WindowPadding.y * 2.0f;

		auto currentCursorPos = ImGui::GetCursorScreenPos();

		ImGui::GetWindowDrawList()->AddRectFilled(
			currentCursorPos,
			ImVec2(
				currentCursorPos.x + ImGui::GetContentRegionAvail().x,
				currentCursorPos.y + ImGui::GetContentRegionAvail().y
			),
			ImGui::GetColorU32(ImGuiCol_ChildBg),
			ImGui::GetStyle().ChildRounding,
			ImDrawFlags_RoundCornersAll
		);

		ImGui::SetCursorScreenPos(
			ImVec2(
				currentCursorPos.x + ImGui::GetStyle().WindowPadding.x,
				currentCursorPos.y + ImGui::GetStyle().WindowPadding.y
			)
		);

		const auto toolbarIconSize = ImVec2(
			toolbarHeight * 2.75f,
			toolbarHeight
		);

		{
			constexpr float iconScale = 1.0f;
			if (
				ImGuiWidgets::IconButton(
					kIconVertex,
					"Vertices",
					toolbarIconSize,
					iconScale,
					ImGuiDir_Right
				)
			) {
				mNotification->PushNotification(
					"未実装",
					"頂点選択はまだ実装されていません。",
					NOTIFY_TYPE::WARNING,
					10.0f
				);
			}
			ImGui::SameLine();
			ImGuiWidgets::IconButton(
				kIconEdge, "Edges", toolbarIconSize, iconScale, ImGuiDir_Right
			);
			ImGui::SameLine();
			ImGuiWidgets::IconButton(
				kIconFace, "Faces", toolbarIconSize, iconScale, ImGuiDir_Right
			);
			ImGui::SameLine();
			ImGuiWidgets::IconButton(
				kIconMesh, "Meshes", toolbarIconSize, iconScale, ImGuiDir_Right
			);
			ImGui::SameLine();
			ImGuiWidgets::IconButton(
				kIconObject, "Object", toolbarIconSize, iconScale, ImGuiDir_Right
			);
			ImGui::SameLine();
			ImGuiWidgets::IconButton(
				kIconGroup, "Group", toolbarIconSize, iconScale, ImGuiDir_Right
			);
		}

		ImGui::SameLine();
		{
			const bool quadLayout = mViewportLayoutMode ==
			                        EDITOR_VIEW_LAYOUT_MODE::QUAD;
			const char* viewportModeLabel = "Fit";
			if (quadLayout) {
				viewportModeLabel = "Fit (Quad)";
			} else {
				switch (mViewportRenderMode) {
					case EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT:
						viewportModeLabel = "Fit";
						break;
					case EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_16_9:
						viewportModeLabel = "16:9";
						break;
					case EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_4_3:
						viewportModeLabel = "4:3";
						break;
					case EDITOR_VIEWPORT_RENDER_MODE::HD720:
						viewportModeLabel = "HD";
						break;
					case EDITOR_VIEWPORT_RENDER_MODE::FHD1080:
						viewportModeLabel = "FHD";
						break;
					default: break;
				}
			}
			ImGui::SetNextItemWidth(140.0f);
			if (quadLayout) {
				ImGui::BeginDisabled();
			}
			if (ImGui::BeginCombo("RenderMode", viewportModeLabel)) {
				if (ImGui::Selectable(
					"Fit",
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT
				)) {
					mViewportRenderMode =
						EDITOR_VIEWPORT_RENDER_MODE::FIT_VIEWPORT;
				}
				if (ImGui::Selectable(
					"Fixed 16:9",
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_16_9
				)) {
					mViewportRenderMode =
						EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_16_9;
				}
				if (ImGui::Selectable(
					"Fixed 4:3",
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_4_3
				)) {
					mViewportRenderMode =
						EDITOR_VIEWPORT_RENDER_MODE::FIXED_ASPECT_4_3;
				}
				if (ImGui::Selectable(
					"HD (1280x720)",
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::HD720
				)) {
					mViewportRenderMode = EDITOR_VIEWPORT_RENDER_MODE::HD720;
				}
				if (ImGui::Selectable(
					"FHD (1920x1080)",
					mViewportRenderMode ==
					EDITOR_VIEWPORT_RENDER_MODE::FHD1080
				)) {
					mViewportRenderMode = EDITOR_VIEWPORT_RENDER_MODE::FHD1080;
				}
				ImGui::EndCombo();
			}
			if (quadLayout) {
				ImGui::EndDisabled();
			}

			ImGui::SameLine();
			const char* layoutLabel =
				mViewportLayoutMode == EDITOR_VIEW_LAYOUT_MODE::QUAD ?
					"Quad" :
					"Single";
			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::BeginCombo("Layout", layoutLabel)) {
				if (ImGui::Selectable(
					"Single",
					mViewportLayoutMode == EDITOR_VIEW_LAYOUT_MODE::SINGLE
				)) {
					mViewportLayoutMode = EDITOR_VIEW_LAYOUT_MODE::SINGLE;
					mActiveViewportViewKey = std::string(kViewScenePerspective);
				}
				if (ImGui::Selectable(
					"Quad",
					mViewportLayoutMode == EDITOR_VIEW_LAYOUT_MODE::QUAD
				)) {
					mViewportLayoutMode = EDITOR_VIEW_LAYOUT_MODE::QUAD;
					mActiveViewportViewKey = std::string(kViewScenePerspective);
				}
				ImGui::EndCombo();
			}

			auto drawViewBindingSelector = [this](
				                              const std::string_view viewKey,
				                              const char*            compactLabel
			                              ) {
				const Scene* scene = GetOutlinerScene();
				ViewportCameraBinding binding = ResolveViewportBinding(viewKey);
				auto setBinding = [this, viewKey](
					                  const ViewportCameraBinding newBinding
				                  ) {
					mCameraManager.SetPaneBinding(viewKey, newBinding);
				};

				const char* currentKindLabel = "EditorPerspective";
				switch (binding.kind) {
					case ViewportCameraBindingKind::EditorPerspective:
						currentKindLabel = "EditorPerspective";
						break;
					case ViewportCameraBindingKind::EditorOrthoTop:
						currentKindLabel = "EditorOrthoTop";
						break;
					case ViewportCameraBindingKind::EditorOrthoFront:
						currentKindLabel = "EditorOrthoFront";
						break;
					case ViewportCameraBindingKind::EditorOrthoRight:
						currentKindLabel = "EditorOrthoRight";
						break;
					case ViewportCameraBindingKind::ActiveGameCamera:
						currentKindLabel = "ActiveGameCamera";
						break;
					case ViewportCameraBindingKind::CameraEntity:
						currentKindLabel = "CameraEntity";
						break;
					default: break;
				}

				ImGui::PushID(std::string(viewKey).c_str());
				ImGui::SetNextItemWidth(132.0f);
				if (ImGui::BeginCombo(compactLabel, currentKindLabel)) {
					const auto selectableKind = [&](
						                            const char*                    label,
						                            const ViewportCameraBindingKind kind
					                            ) {
						if (ImGui::Selectable(label, binding.kind == kind)) {
							ViewportCameraBinding next = binding;
							next.kind                 = kind;
							if (kind != ViewportCameraBindingKind::CameraEntity) {
								next.cameraEntityGuid = 0;
							}
							setBinding(next);
							binding = next;
						}
					};
					selectableKind(
						"EditorPerspective",
						ViewportCameraBindingKind::EditorPerspective
					);
					selectableKind(
						"EditorOrthoTop",
						ViewportCameraBindingKind::EditorOrthoTop
					);
					selectableKind(
						"EditorOrthoFront",
						ViewportCameraBindingKind::EditorOrthoFront
					);
					selectableKind(
						"EditorOrthoRight",
						ViewportCameraBindingKind::EditorOrthoRight
					);
					selectableKind(
						"ActiveGameCamera",
						ViewportCameraBindingKind::ActiveGameCamera
					);
					selectableKind(
						"CameraEntity",
						ViewportCameraBindingKind::CameraEntity
					);
					ImGui::EndCombo();
				}

				if (binding.kind == ViewportCameraBindingKind::CameraEntity) {
					std::string cameraLabel = "<none>";
					if (scene && binding.cameraEntityGuid != 0) {
						for (const auto& entity : scene->GetEntities()) {
							if (!entity || entity->GetGuid() != binding.cameraEntityGuid) {
								continue;
							}
							cameraLabel = std::string(entity->GetName());
							break;
						}
					}

					ImGui::SetNextItemWidth(132.0f);
					if (ImGui::BeginCombo("##CameraEntity", cameraLabel.c_str())) {
						if (ImGui::Selectable("<none>", binding.cameraEntityGuid == 0)) {
							ViewportCameraBinding next = binding;
							next.cameraEntityGuid = 0;
							setBinding(next);
							binding = next;
						}
						if (scene) {
							for (const auto& entity : scene->GetEntities()) {
								if (!entity || !entity->IsActive()) {
									continue;
								}
								const auto* camera = entity->GetComponent<CameraComponent>();
								if (
									!camera ||
									!camera->IsActive() ||
									!camera->IsCameraActive()
								) {
									continue;
								}
								const bool selected =
									entity->GetGuid() == binding.cameraEntityGuid;
								const std::string label = std::format(
									"{}##{}",
									entity->GetName(),
									entity->GetGuid()
								);
								if (ImGui::Selectable(label.c_str(), selected)) {
									ViewportCameraBinding next = binding;
									next.cameraEntityGuid      = entity->GetGuid();
									setBinding(next);
									binding = next;
								}
							}
						}
						ImGui::EndCombo();
					}
				}
				ImGui::PopID();
			};

			if (mViewportLayoutMode == EDITOR_VIEW_LAYOUT_MODE::QUAD) {
				ImGui::SameLine();
				ImGui::TextUnformatted("P");
				ImGui::SameLine();
				drawViewBindingSelector(kViewScenePerspective, "##QuadPerspective");
				ImGui::SameLine();
				ImGui::TextUnformatted("T");
				ImGui::SameLine();
				drawViewBindingSelector(kViewSceneTop, "##QuadTop");
				ImGui::SameLine();
				ImGui::TextUnformatted("F");
				ImGui::SameLine();
				drawViewBindingSelector(kViewSceneFront, "##QuadFront");
				ImGui::SameLine();
				ImGui::TextUnformatted("R");
				ImGui::SameLine();
				drawViewBindingSelector(kViewSceneRight, "##QuadRight");
			} else {
				ImGui::SameLine();
				drawViewBindingSelector(
					kViewScenePerspective, "##SinglePerspectiveBinding"
				);
			}
		}

		ImGui::SetCursorScreenPos(
			{
				currentCursorPos.x,
				currentCursorPos.y + toolbarHeightWithPadding
			}
		);
	}

	void LevelEditorTool::DrawViewport(const float deltaTime) {
		if (!ImGui::Begin("Viewport")) {
			ImGui::End();
			return;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
		DrawViewportTopBar();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		const ImVec2 avail  = ImGui::GetContentRegionAvail();
		const float  width  = std::max(1.0f, avail.x);
		const float  height = std::max(1.0f, avail.y);
		mViewportSizeChangedThisFrame =
			std::abs(width - mLastViewportSize.x) > 0.5f ||
			std::abs(height - mLastViewportSize.y) > 0.5f;
		mViewportPanelWidth  = width;
		mViewportPanelHeight = height;
		mLastViewportSize    = Vec2(width, height);

		auto drawPane = [this](
			                const std::string_view viewKey,
			                const char*            label,
			                const float            drawWidth,
			                const float            drawHeight
		                ) {
			const ImVec2 panePos   = ImGui::GetCursorScreenPos();
			const auto outputIt   = mViewOutputs.find(std::string(viewKey));
			const bool hasOutput  = outputIt != mViewOutputs.end();
			const Vec2 outputSize = hasOutput ?
				                        outputIt->second.size :
				                        Vec2(drawWidth, drawHeight);

			const float safeOutputWidth  = std::max(1.0f, outputSize.x);
			const float safeOutputHeight = std::max(1.0f, outputSize.y);
			const float outputAspect     = safeOutputWidth / safeOutputHeight;

			float fitWidth  = drawWidth;
			float fitHeight = fitWidth / outputAspect;
			if (fitHeight > drawHeight) {
				fitHeight = drawHeight;
				fitWidth  = fitHeight * outputAspect;
			}
			fitWidth  = std::max(1.0f, fitWidth);
			fitHeight = std::max(1.0f, fitHeight);
			const float fitOffsetX = (drawWidth - fitWidth) * 0.5f;
			const float fitOffsetY = (drawHeight - fitHeight) * 0.5f;
			const ImVec2 fitPos = ImVec2(
				panePos.x + fitOffsetX, panePos.y + fitOffsetY
			);
			const ImVec2 fitMax = ImVec2(
				fitPos.x + fitWidth, fitPos.y + fitHeight
			);

			ImGui::InvisibleButton(
				(std::string("##") + std::string(viewKey)).c_str(),
				ImVec2(drawWidth, drawHeight)
			);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(
				panePos,
				ImVec2(panePos.x + drawWidth, panePos.y + drawHeight),
				IM_COL32(28, 30, 36, 255),
				8.0f
			);
			drawList->AddRect(
				panePos,
				ImVec2(panePos.x + drawWidth, panePos.y + drawHeight),
				IM_COL32(70, 74, 86, 255),
				8.0f
			);

			if (
				hasOutput &&
				outputIt->second.srvCpu.ptr != 0 &&
				outputIt->second.textureId != 0
			) {
				const ImTextureID tex = mImGuiLayer.GetOrCreateTextureId(
					outputIt->second.textureId,
					outputIt->second.srvRevision,
					outputIt->second.srvCpu
				);
				drawList->AddImage(
					tex,
					fitPos,
					fitMax,
					ImVec2(0.0f, 0.0f),
					ImVec2(1.0f, 1.0f)
				);
			} else {
				drawList->AddRect(
					fitPos,
					fitMax,
					IM_COL32(130, 130, 130, 255),
					0.0f
				);
			}

			const ImVec2 mousePos = ImGui::GetMousePos();
			const bool hoveredFit = mousePos.x >= fitPos.x && mousePos.x <= fitMax.x &&
			                        mousePos.y >= fitPos.y && mousePos.y <= fitMax.y &&
			                        ImGui::IsWindowHovered(
			                        	ImGuiHoveredFlags_AllowWhenBlockedByPopup
			                        );
			if (hoveredFit && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				mActiveViewportViewKey = std::string(viewKey);
				mViewportLookActive    = true;
			}
			if (
				hoveredFit ||
				(!mActiveViewportViewKey.empty() && mActiveViewportViewKey == viewKey)
			) {
				mViewportPosition = Vec2(fitPos.x, fitPos.y);
				mViewportSize     = Vec2(fitWidth, fitHeight);
			}

			if (hoveredFit) {
				const ViewportCameraBinding binding = ResolveViewportBinding(viewKey);
				const bool isOrtho =
					binding.kind == ViewportCameraBindingKind::EditorOrthoTop ||
					binding.kind == ViewportCameraBindingKind::EditorOrthoFront ||
					binding.kind == ViewportCameraBindingKind::EditorOrthoRight;
				if (isOrtho) {
					ViewportOrthoState ortho = mCameraManager.GetOrthoState(viewKey);
					const ImGuiIO&      io    = ImGui::GetIO();
					if (std::abs(io.MouseWheel) > 0.0f) {
						const float scale = std::pow(0.9f, io.MouseWheel);
						ortho.zoom        = std::clamp(ortho.zoom * scale, 16.0f, 100000.0f);
					}
					if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
						const float worldPerPixelX =
							std::max(1.0f, ortho.zoom * (fitWidth / std::max(1.0f, fitHeight))) /
							std::max(1.0f, fitWidth);
						const float worldPerPixelY = ortho.zoom / std::max(1.0f, fitHeight);
						const Vec2  dragDelta      = Vec2(io.MouseDelta.x, io.MouseDelta.y);

						Vec3 rightAxis = Vec3::right;
						Vec3 upAxis    = Vec3::up;
						if (binding.kind == ViewportCameraBindingKind::EditorOrthoTop) {
							rightAxis = Vec3::right;
							upAxis    = Vec3::forward;
						} else if (
							binding.kind == ViewportCameraBindingKind::EditorOrthoFront
						) {
							rightAxis = Vec3::left;
							upAxis    = Vec3::up;
						} else {
							rightAxis = Vec3::forward;
							upAxis    = Vec3::up;
						}

						ortho.center -= rightAxis * (dragDelta.x * worldPerPixelX);
						ortho.center += upAxis * (dragDelta.y * worldPerPixelY);
					}
					mCameraManager.SetOrthoState(viewKey, ortho);
				}
			}

			drawList->AddText(
				ImVec2(panePos.x + 8.0f, panePos.y + 8.0f),
				IM_COL32(240, 240, 240, 255),
				label
			);

			return std::tuple<bool, Vec2, Vec2>(
				hoveredFit,
				Vec2(fitPos.x, fitPos.y),
				Vec2(fitWidth, fitHeight)
			);
		};

		if (mViewportLayoutMode == EDITOR_VIEW_LAYOUT_MODE::QUAD) {
			const float paneW = std::max(1.0f, width * 0.5f);
			const float paneH = std::max(1.0f, height * 0.5f);
			const ImVec2 baseCursor = ImGui::GetCursorPos();

			struct PaneDesc {
				std::string_view key;
				const char*      label;
				ImVec2           offset;
			};
			const PaneDesc panes[4] = {
				{kViewScenePerspective, "Perspective", ImVec2(0.0f, 0.0f)},
				{kViewSceneTop, "Top", ImVec2(paneW, 0.0f)},
				{kViewSceneFront, "Front", ImVec2(0.0f, paneH)},
				{kViewSceneRight, "Right", ImVec2(paneW, paneH)},
			};

			for (const PaneDesc& pane : panes) {
				ImGui::SetCursorPos(
					ImVec2(baseCursor.x + pane.offset.x, baseCursor.y + pane.offset.y)
				);
				const auto [hovered, pos, size] = drawPane(
					pane.key, pane.label, paneW, paneH
				);
				if (mActiveViewportViewKey == pane.key) {
					const Render::SceneViewRenderMode mode =
						BuildSceneViewModeForSize(size.x, size.y, true);
					DrawViewportGizmo(pane.key, mode, pos, size.x, size.y);
				}
				(void)hovered;
			}

			ImGui::SetCursorPos(
				ImVec2(baseCursor.x + width, baseCursor.y + height)
			);
		} else {
			const auto [hovered, pos, size] = drawPane(
				kViewScenePerspective, "Perspective", width, height
			);
			(void)hovered;
			const Render::SceneViewRenderMode mode = BuildSceneViewModeForSize(
				size.x, size.y, false
			);
			DrawViewportGizmo(
				kViewScenePerspective, mode, pos, size.x, size.y
			);
		}

		if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
			mViewportLookActive = false;
		}

		DrawViewportOverlay(deltaTime);

		ImGui::PopStyleVar(2);
		ImGui::End();
	}

	void LevelEditorTool::DrawViewportOverlay(const float deltaTime) {
		const EditorCameraComponent* camera = mEditorWorld.GetEditorCamera();
		if (!camera || !camera->IsMoveSpeedPopupVisible()) {
			return;
		}

		constexpr float windowMinWidth = 256.0f;

		const std::string text =
			StrUtil::ConvertToUtf8(kIconSpeed) +
			std::format(" {:.2f}", camera->GetMoveSpeed());

		const auto textSize      = ImGui::CalcTextSize(text.c_str());
		const auto windowPadding = ImGui::GetStyle().WindowPadding;

		const ImVec2 windowSize(
			std::max(textSize.x, windowMinWidth) + windowPadding.x,
			textSize.y + windowPadding.y
		);

		ImVec2 windowPos(
			mViewportPosition.x + mViewportSize.x * 0.5f,
			mViewportPosition.y + mViewportSize.y * 0.9f
		);
		windowPos.x -= windowSize.x * 0.5f;
		windowPos.y -= windowSize.y - 8.0f;

		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

		constexpr float fadeOutStart       = 0.5f;
		constexpr float invFadeOutDuration = 1.0f / (1.0f - fadeOutStart);

		const float alphaMultiplier =
			camera->GetMoveSpeedPopupTimer() < fadeOutStart ?
				1.0f :
				1.0f - (camera->GetMoveSpeedPopupTimer() - fadeOutStart) *
				invFadeOutDuration;

		const float alpha = Math::CubicBezier(
			alphaMultiplier,
			0.2f, 0.0f, 0.0f, 1.0f
		);

		ImGui::SetNextWindowBgAlpha(alpha * 0.9f);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
		ImGui::PushStyleVar(
			ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f)
		);

		ImGui::Begin(
			"##editorCameraMoveSpeedPopup", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoInputs
		);

		ImGui::SetCursorPos(
			ImVec2(
				(windowSize.x - textSize.x) * 0.5f,
				(windowSize.y - textSize.y) * 0.25f
			)
		);
		ImGui::TextUnformatted(text.c_str());

		static float currentProgress = 0.0f;
		float        progress;
		if (camera->GetMoveSpeed() <= 0.125f) {
			progress = 0.0f;
		} else if (camera->GetMoveSpeed() >= 65536.0f) {
			progress = 1.0f;
		} else {
			const float logMin   = std::log2(0.125f);
			const float logMax   = std::log2(65536.0f);
			const float logValue = std::log2(camera->GetMoveSpeed());
			progress             = (logValue - logMin) / (logMax - logMin);
		}

		const float t = Math::CubicBezier(
			20.0f * deltaTime, 0.2f, 0.0f, 0.0f, 1.0f
		);

		currentProgress = std::lerp(currentProgress, progress, t);

		ImGui::SetCursorPosY(
			ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y - 4.0f
		);

		ImGui::ProgressBar(
			currentProgress, ImVec2(
				ImGui::GetContentRegionAvail().x,
				4.0f
			),
			""
		);

		ImGui::End();
		ImGui::PopStyleVar(2);
	}
}

#endif

