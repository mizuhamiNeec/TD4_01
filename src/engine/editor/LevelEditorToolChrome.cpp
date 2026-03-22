#ifdef _DEBUG
#include "LevelEditorTool.h"

#include <array>
#include <imgui.h>
#include <imgui_internal.h>
#include <utility>

#include "core/string/StrUtil.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "engine/world/EditorWorld.h"

namespace Unnamed {
	//-------------------------------------------------------------------------
	// Chromeは某ブラウザではなく、UIの外枠という意味なのじゃ。
	//-------------------------------------------------------------------------

	void LevelEditorTool::DrawMainMenu() {
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				const auto currentPath =
					std::string(mEditorWorld.GetLoadedScenePath());

				if (ImGui::MenuItem("Save")) {
					const std::string savePath = currentPath.empty() ?
						                             "./content/core/scenes/sandbox.json" :
						                             currentPath;
					SaveSceneAs(savePath);
				}

				if (ImGui::MenuItem("Save As (sandbox.json)")) {
					SaveSceneAs("./content/core/scenes/sandbox.json");
				}

				ImGui::Separator();

				(void)ImGui::MenuItem("Import");
				(void)ImGui::MenuItem("Export");
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}
	}

	void LevelEditorTool::DrawSideBar() {
		constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |           // タイトルバーなし
			ImGuiWindowFlags_NoScrollbar |          // スクロールバーなし
			ImGuiWindowFlags_NoCollapse |           // 折りたたみなし
			ImGuiWindowFlags_NoResize |             // サイズ変更なし
			ImGuiWindowFlags_NoSavedSettings |      // 設定保存なし
			ImGuiWindowFlags_NoFocusOnAppearing |   // 表示時にフォーカスなし
			ImGuiWindowFlags_NoBringToFrontOnFocus; // フォーカス時に前面に出ない

		constexpr float iconSize  = 40.0f;
		constexpr float iconScale = 0.8f;

		constexpr auto toolbarIconSize = ImVec2(iconSize, iconSize);

		const auto windowPadding = ImGui::GetStyle().WindowPadding;

		ImGui::SetNextWindowBgAlpha(1.0f);
		if (
			ImGui::BeginViewportSideBar(
				"##LeftToolbar", ImGui::GetMainViewport(), ImGuiDir_Left,
				toolbarIconSize.x + windowPadding.x * 2.0f, // アイコン幅 + 左右パディング
				flags
			)
		) {
			// ツールバー
			{
				ImGuiWidgets::IconButton(
					kIconSelect,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconDragPan,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconRotate,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconScale,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconPivot,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconObject,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconBox,
					"",
					toolbarIconSize,
					iconScale
				);

				ImGuiWidgets::IconButton(
					kIconTexture,
					"",
					toolbarIconSize,
					iconScale
				);
			}
			ImGui::End();
		}
	}

	void LevelEditorTool::DrawStatusBar() {
		constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |           // タイトルバーなし
			ImGuiWindowFlags_NoScrollbar |          // スクロールバーなし
			ImGuiWindowFlags_NoCollapse |           // 折りたたみなし
			ImGuiWindowFlags_NoResize |             // サイズ変更なし
			ImGuiWindowFlags_NoSavedSettings |      // 設定保存なし
			ImGuiWindowFlags_NoFocusOnAppearing |   // 表示時にフォーカスなし
			ImGuiWindowFlags_NoBringToFrontOnFocus; // フォーカス時に前面に出ない

		if (
			ImGui::BeginViewportSideBar(
				"##MainStatusBar",
				ImGui::GetMainViewport(),
				ImGuiDir_Down,
				32.0f,
				flags
			)
		) {
			const float statusBarWidth = ImGui::GetWindowWidth();

			// アングルスナップ
			{
				// 5.625° 360°の64分割
				// 11.25°は360°の32分割
				// 22.5°は360°の16分割
				constexpr std::array items = {
					"0.25°", "0.5°", "1°",
					"5°", "5.625°", "11.25°",
					"15°", "22.5°", "30°",
					"45°", "90°"
				};
				static uint32_t itemCurrentIndex = 6u; // デフォルトは15°
				const char*     comboLabel       = items[itemCurrentIndex];

				ImGui::Text("Angle: ");

				ImGui::SameLine();

				// コンボボックスの幅をステータスバーの幅に合わせて調整
				ImGui::PushItemWidth(statusBarWidth * 0.2f);
				ImGui::PushID("AngleSnapCombo");
				if (ImGui::BeginCombo("##angleSnap", comboLabel)) {
					for (int n = 0; n < items.size(); ++n) {
						const bool isSelected =
							std::cmp_equal(itemCurrentIndex, n);
						if (ImGui::Selectable(items[n], isSelected)) {
							itemCurrentIndex = n;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				constexpr auto size = static_cast<uint32_t>(items.size());
				ImGuiWidgets::HandleHoveredComboMenuMouseWheelScroll(
					itemCurrentIndex, size
				);
				// 選択された文字列を浮動小数点数に変換してangleSnapに設定
				mAngleSnapDegree = std::stof(items[itemCurrentIndex]);
				ImGui::PopID();
				ImGui::PopItemWidth();
			}

			ImGui::SameLine();

			// グリッドスナップ
			{
				constexpr std::array items = {
					"0.125", "0.25", "0.5",
					"1", "2", "4", "8",
					"16", "32", "64", "128",
					"256", "512", "1024"
				};
				static uint32_t itemCurrentIndex = 9u; // デフォルトは64
				const char*     comboLabel       = items[itemCurrentIndex];

				ImGui::Text("Grid: ");

				ImGui::SameLine();

				// コンボボックスの幅をステータスバーの幅に合わせて調整
				ImGui::PushItemWidth(statusBarWidth * 0.2f);
				ImGui::PushID("GridSnapCombo");
				if (ImGui::BeginCombo("##gridSnap", comboLabel)) {
					for (int n = 0; n < items.size(); ++n) {
						const bool isSelected =
							std::cmp_equal(itemCurrentIndex, n);
						if (ImGui::Selectable(items[n], isSelected)) {
							itemCurrentIndex = n;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				constexpr auto size = static_cast<uint32_t>(items.size());
				ImGuiWidgets::HandleHoveredComboMenuMouseWheelScroll(
					itemCurrentIndex, size

				);
				// 選択された文字列を浮動小数点数に変換してgridSnapに設定
				mGridSnap = std::stof(items[itemCurrentIndex]);
				ImGui::PopID();
				ImGui::PopItemWidth();
			}

			ImGui::End();
		}
	}
}

#endif

