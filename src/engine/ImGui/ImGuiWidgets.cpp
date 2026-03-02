#ifdef _DEBUG
#include "ImGuiWidgets.h"

#include <array>
#include <imgui.h>
#include <imgui_internal.h>
#include <pch.h>

#include <core/math/Math.h>

#include <engine/Properties.h>
#include <engine/ImGui/Icons.h>

namespace ImGuiWidgets {
	/// @brief Dragウィジェット用のスタイルカラーをプッシュします。
	/// @param bg 通常時の背景色
	/// @param bgHovered ホバー時の背景色
	/// @param bgActive アクティブ時の背景色
	void PushStyleColorForDrag(
		const ImVec4& bg, const ImVec4& bgHovered,
		const ImVec4& bgActive
	) {
		ImGui::PushStyleColor(ImGuiCol_FrameBg, bg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bgHovered);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, bgActive);
	}

	/// @brief Vec3型のドラッグウィジェットを表示します。
	/// @param name ウィジェットのラベル
	/// @param value 編集するVec3型の値への参照
	/// @param defaultValue リセット時のデフォルト値
	/// @param vSpeed ドラッグ操作の速度
	/// @param format 表示フォーマット
	/// @return 値が変更された場合にtrueを返します。
	bool DragVec3(
		const std::string& name,
		Vec3&              value,
		const Vec3         defaultValue,
		const float        vSpeed,
		const char*        format
	) {
		if (ImGui::GetCurrentWindow()->SkipItems) { return false; }

		bool valueChanged = false;

		// 全体のウィジェット幅は通常の DragFloat3 と同じ
		const float widgetWidth = ImGui::GetContentRegionAvail().x;
		const float setWidth    = widgetWidth / 3.0f; // 各軸のセル幅

		// 角丸四角形のサイズ ImGuiから取ってくる
		const float rectWidth  = ImGui::GetFrameHeight();
		const float rectHeight = rectWidth;
		const float rounding   = ImGui::GetStyle().FrameRounding;
		const float dragWidth  = setWidth - rectWidth; // 各 DragFloat の幅

		static constexpr std::array rectColors = {
			IM_COL32(50, 43, 43, 255),
			IM_COL32(43, 45, 39, 255),
			IM_COL32(45, 48, 51, 255)
		};

		static constexpr std::array textColors = {
			IM_COL32(226, 110, 110, 255),
			IM_COL32(168, 204, 96, 255),
			IM_COL32(132, 181, 230, 255)
		};

		// リセットアイコンの文字列を作成 (UTF-8)
		const std::string iconResetStr = Unnamed::StrUtil::ConvertToUtf8(
			kIconReset
		);

		ImGui::PushID(name.c_str());
		ImGui::BeginGroup();
		{
			for (int i = 0; i < 3; i++) {
				ImGui::PushID(i);
				// Shiftキーが押されている場合は全要素をリセット
				if (ImGui::InvisibleButton(
					"##reset",
					ImVec2(rectWidth, rectHeight)
				)) {
					if (ImGui::GetIO().KeyShift) {
						for (int j = 0; j < 3; ++j) {
							value[j] = defaultValue[j];
						}
						valueChanged = true;
					} else {
						value[i]     = defaultValue[i];
						valueChanged = true;
					}
				}
				const bool isHovered = ImGui::IsItemHovered(); // マウスオーバー状態を取得

				ImVec2 rectPos = ImGui::GetItemRectMin();
				ImVec2 rectMax(rectPos.x + rectWidth, rectPos.y + rectHeight);
				ImGui::GetWindowDrawList()->AddRectFilled(
					rectPos,
					rectMax,
					rectColors[i],
					rounding,
					ImDrawFlags_RoundCornersLeft
				);
				// 角丸四角形の枠線を描画
				if (ImGui::GetStyle().FrameBorderSize > 0.0f) {
					ImGui::GetWindowDrawList()->AddRect(
						rectPos,
						rectMax,
						ImGui::GetColorU32(ImGuiCol_Border),
						rounding,
						ImDrawFlags_RoundCornersLeft,
						ImGui::GetStyle().FrameBorderSize
					);
				}

				constexpr std::array<const char*, 3> axisLabels = {
					"X", "Y", "Z"
				};
				if (ImGui::GetIO().KeyShift || isHovered) {
					// Shiftキーが押されている間はリセットアイコンを表示
					const ImVec2 iconSize = ImGui::CalcTextSize(
						iconResetStr.c_str()
					);
					ImVec2 iconPos(
						rectPos.x + (rectWidth - iconSize.x) * 0.5f,
						rectPos.y + (rectHeight - iconSize.y) * 0.5f
					);
					ImGui::GetWindowDrawList()->AddText(
						iconPos, textColors[i], iconResetStr.c_str()
					);
				} else {
					// 通常時は軸ラベルを表示
					const ImVec2 labelSize = ImGui::CalcTextSize(axisLabels[i]);
					ImVec2       labelPos(
						rectPos.x + (rectWidth - labelSize.x) * 0.5f,
						rectPos.y + (rectHeight - labelSize.y) * 0.5f
					);
					ImGui::GetWindowDrawList()->AddText(
						labelPos, textColors[i], axisLabels[i]
					);
				}

				// 角丸四角形と DragFloat の間に隙間を作らない
				ImGui::SameLine(0, 0);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
				ImGui::SetNextItemWidth(dragWidth - 2);
				valueChanged |= ImGui::DragFloat(
					"", &value[i], vSpeed, 0, 0, format
				);
				ImGui::PopStyleVar();

				if (i < 2) {
					ImGui::SameLine(0, 4); // 各セットの間隔
				}
				ImGui::PopID();
			}
		}
		ImGui::EndGroup();
		ImGui::PopID();

		// 右側に通常通りラベルを表示
		ImGui::SameLine();
		ImGui::Text("%s", name.c_str());

		return valueChanged;
	}

	/// @brief Cubic Bézier曲線の編集ウィジェットを表示します。
	/// @param label ウィジェットのラベル
	/// @param p0 コントロールポイント1のX座標への参照
	/// @param p1 コントロールポイント1のY座標への参照
	/// @param p2 コントロールポイント2のX座標への参照
	/// @param p3 コントロールポイント2のY座標への参照
	/// @return 値が変更された場合にtrueを返します。
	bool EditCubicBezier(
		const std::string& label, float p0, float p1, float p2, float p3
	) {
		// ドラッグ中か?
		static bool bIsDraggingCp1 = false;
		static bool bIsDraggingCp2 = false;

		float controlPoints[] = {p0, p1, p2, p3};

		ImGui::DragFloat4(label.c_str(), controlPoints, 0.01f, 0.0f, 1.0f);

		// ウィンドウ内の利用可能領域から正方形のキャンバスサイズを決定
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const float  side  = avail.x < avail.y ? avail.x : avail.y;
		const ImVec2 canvasSize(side, side);
		const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("canvas", canvasSize);

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// キャンバスの背景描画
		drawList->AddRectFilled(
			canvasPos,
			ImVec2(
				canvasPos.x + canvasSize.x,
				canvasPos.y + canvasSize.y
			),
			IM_COL32(50, 50, 50, 255)
		);

		// Cubic Bézierの各点を計算・描画 （始点:P0=(0,0), 終点:P3=(1,1)固定）
		constexpr int steps = 64;
		ImVec2        prevPoint;
		bool          first = true;
		for (int i = 0; i < steps; ++i) {
			const float t         = static_cast<float>(i) / (steps - 1);
			const float oneMinusT = 1.0f - t;
			const float bx        = oneMinusT * oneMinusT * oneMinusT * 0.0f +
			                        3 * oneMinusT * oneMinusT * t *
			                        controlPoints[0] +
			                        3 * oneMinusT * t * t * controlPoints[2] +
			                        t * t * t * 1.0f;
			const float by = oneMinusT * oneMinusT * oneMinusT * 0.0f +
			                 3 * oneMinusT * oneMinusT * t * controlPoints[1] +
			                 3 * oneMinusT * t * t * controlPoints[3] +
			                 t * t * t * 1.0f;
			auto point = ImVec2(
				canvasPos.x + bx * canvasSize.x,
				canvasPos.y + (1.0f - by) * canvasSize.y
			);

			if (!first) {
				drawList->AddLine(
					prevPoint, point, IM_COL32(255, 255, 0, 255),
					2.0f
				);
			}
			prevPoint = point;
			first     = false;
		}

		// 各コントロールポイントのキャンバス上の位置を計算
		const auto cp1 = ImVec2(
			canvasPos.x + controlPoints[0] * canvasSize.x,
			canvasPos.y + (1.0f - controlPoints[1]) * canvasSize.y
		);
		const auto cp2 = ImVec2(
			canvasPos.x + controlPoints[2] * canvasSize.x,
			canvasPos.y + (1.0f - controlPoints[3]) * canvasSize.y
		);

		// 円描画（各コントロールポイント）
		constexpr float handleRadius = 16.0f;
		drawList->AddCircleFilled(cp1, handleRadius, IM_COL32(255, 0, 0, 255));
		drawList->AddCircleFilled(cp2, handleRadius, IM_COL32(255, 0, 0, 255));

		// マウス操作による直接操作
		const ImGuiIO& io       = ImGui::GetIO();
		const ImVec2   mousePos = io.MousePos;

		auto distance = [](const ImVec2& a, const ImVec2& b) -> float {
			const float dx = a.x - b.x;
			const float dy = a.y - b.y;
			return sqrtf(dx * dx + dy * dy);
		};

		// マウスクリック判定：クリック時、各コントロールポイントの円領域内ならドラッグ開始
		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
			if (distance(mousePos, cp1) <= handleRadius * 1.5f) {
				bIsDraggingCp1 = true;
			} else if (distance(mousePos, cp2) <= handleRadius * 1.5f) {
				bIsDraggingCp2 = true;
			}
		}
		// マウスボタンリリースでドラッグ終了
		if (!io.MouseDown[0]) { bIsDraggingCp1 = bIsDraggingCp2 = false; }

		// ドラッグ中なら、マウス位置に応じてコントロールポイントを更新（キャンバス座標を[0,1]に変換）
		auto clamp = [
			](const float v, const float lo, const float hi) -> float {
			return v < lo ? lo : v > hi ? hi : v;
		};
		if (bIsDraggingCp1) {
			const float nx = clamp(
				(mousePos.x - canvasPos.x) / canvasSize.x,
				0.0f, 1.0f
			);
			const float ny = clamp(
				(mousePos.y - canvasPos.y) / canvasSize.y,
				0.0f, 1.0f
			);
			// Y軸は反転しているため
			controlPoints[0] = nx;
			controlPoints[1] = 1.0f - ny;
		}
		if (bIsDraggingCp2) {
			const float nx = clamp(
				(mousePos.x - canvasPos.x) / canvasSize.x,
				0.0f, 1.0f
			);
			const float ny = clamp(
				(mousePos.y - canvasPos.y) / canvasSize.y,
				0.0f, 1.0f
			);
			controlPoints[2] = nx;
			controlPoints[3] = 1.0f - ny;
		}

		p0 = controlPoints[0];
		p1 = controlPoints[1];
		p2 = controlPoints[2];
		p3 = controlPoints[3];

		return bIsDraggingCp1 || bIsDraggingCp2;
	}

	bool IconButton(
		const uint32_t icon,
		const char*    label,
		ImVec2         size,
		const float    iconScale,
		const ImGuiDir labelDir
	) {
		const ImGuiStyle& style = ImGui::GetStyle();
		ImDrawList*       dl = ImGui::GetWindowDrawList();
		ImFont*           font = ImGui::GetFont();
		const std::string iconUtf8 = Unnamed::StrUtil::ConvertToUtf8(icon);
		const bool        hasLabel = label && label[0] != '\0';
		const bool        horizontalLayout =
			labelDir == ImGuiDir_Left || labelDir == ImGuiDir_Right;
		const bool  iconOnly = !hasLabel || labelDir == ImGuiDir_None;
		const float gap      = horizontalLayout ?
			                       style.ItemSpacing.x :
			                       style
			                       .ItemSpacing.y;
		const ImVec2 pad          = style.FramePadding;
		const ImVec2 iconBaseSize = ImGui::CalcTextSize(iconUtf8.c_str());
		const ImVec2 labelSize    = hasLabel ?
			                            ImGui::CalcTextSize(label) :
			                            ImVec2(0.0f, 0.0f);

		if (size.x <= 0.0f) {
			if (iconOnly) { size.x = iconBaseSize.x + pad.x * 2.0f; } else if (
				horizontalLayout) {
				size.x = iconBaseSize.x + labelSize.x + gap + pad.x * 2.0f;
			} else {
				size.x = std::max(iconBaseSize.x, labelSize.x) + pad.x * 2.0f;
			}
		}

		if (size.y <= 0.0f) {
			if (iconOnly) { size.y = iconBaseSize.y + pad.y * 2.0f; } else if (
				horizontalLayout) {
				size.y = std::max(iconBaseSize.y, labelSize.y) + pad.y * 2.0f;
			} else {
				size.y = iconBaseSize.y + labelSize.y + gap + pad.y * 2.0f;
			}
		}

		const std::string btnId =
			"##IconBtn" + iconUtf8 + std::to_string(ImGui::GetID(icon));
		const bool pressed = ImGui::InvisibleButton(btnId.c_str(), size);
		const bool hovered = ImGui::IsItemHovered();
		const bool active  = ImGui::IsItemActive();

		const ImVec2 itemMin  = ImGui::GetItemRectMin();
		const ImVec2 itemMax  = ImGui::GetItemRectMax();
		const ImVec2 itemSize = ImVec2(
			itemMax.x - itemMin.x, itemMax.y - itemMin.y
		);
		const ImVec2 innerMin  = ImVec2(itemMin.x + pad.x, itemMin.y + pad.y);
		const ImVec2 innerSize = ImVec2(
			std::max(1.0f, itemSize.x - pad.x * 2.0f),
			std::max(1.0f, itemSize.y - pad.y * 2.0f)
		);

		const ImU32 bgColor = ImGui::GetColorU32(
			active ?
				ImGuiCol_ButtonActive :
				hovered ?
				ImGuiCol_ButtonHovered :
				ImGuiCol_Button
		);
		ImGui::RenderFrame(
			itemMin, itemMax, bgColor, true, style.FrameRounding
		);

		const float  baseFontSize = ImGui::GetFontSize();
		const float  iconFontSize = std::max(1.0f, innerSize.y * iconScale);
		const float  fontScale    = iconFontSize / std::max(1.0f, baseFontSize);
		const ImVec2 iconSize     = ImVec2(
			iconBaseSize.x * fontScale, iconBaseSize.y * fontScale
		);

		ImVec2 iconPos  = innerMin;
		ImVec2 labelPos = innerMin;

		if (iconOnly) {
			iconPos = ImVec2(
				innerMin.x + (innerSize.x - iconSize.x) * 0.5f,
				innerMin.y + (innerSize.y - iconSize.y) * 0.5f
			);
		} else if (horizontalLayout) {
			const float blockWidth = iconSize.x + labelSize.x + gap;
			const float blockHeight = std::max(iconSize.y, labelSize.y);
			const float startX = innerMin.x + (innerSize.x - blockWidth) * 0.5f;
			const float startY =
				innerMin.y + (innerSize.y - blockHeight) * 0.5f;

			if (labelDir == ImGuiDir_Left) {
				labelPos = ImVec2(
					startX, startY + (blockHeight - labelSize.y) * 0.5f
				);
				iconPos = ImVec2(
					startX + labelSize.x + gap,
					startY + (blockHeight - iconSize.y) * 0.5f
				);
			} else {
				iconPos = ImVec2(
					startX, startY + (blockHeight - iconSize.y) * 0.5f
				);
				labelPos = ImVec2(
					startX + iconSize.x + gap,
					startY + (blockHeight - labelSize.y) * 0.5f
				);
			}
		} else {
			const float blockHeight = iconSize.y + labelSize.y + gap;
			const float startY      =
				innerMin.y + (innerSize.y - blockHeight) * 0.5f;
			if (labelDir == ImGuiDir_Up) {
				labelPos = ImVec2(
					innerMin.x + (innerSize.x - labelSize.x) * 0.5f,
					startY
				);
				iconPos = ImVec2(
					innerMin.x + (innerSize.x - iconSize.x) * 0.5f,
					startY + labelSize.y + gap
				);
			} else {
				iconPos = ImVec2(
					innerMin.x + (innerSize.x - iconSize.x) * 0.5f,
					startY
				);
				labelPos = ImVec2(
					innerMin.x + (innerSize.x - labelSize.x) * 0.5f,
					startY + iconSize.y + gap
				);
			}
		}

		const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
		dl->PushClipRect(itemMin, itemMax, true);
		dl->AddText(font, iconFontSize, iconPos, textColor, iconUtf8.c_str());
		if (!iconOnly) { dl->AddText(labelPos, textColor, label); }
		dl->PopClipRect();

		return pressed;
	}

	bool MenuItemWithIcon(
		const char*    label,
		const uint32_t icon,
		const char*    shortcut,
		const bool     selected,
		const bool     enabled,
		const float    rounding
	) {
		return MenuItemExWithRounding(
			label,
			Unnamed::StrUtil::ConvertToUtf8(icon).c_str(),
			shortcut,
			selected,
			enabled,
			rounding
		);
	}

	/// @brief メインメニュー用のBeginMenuを開始します。
	///	@param label メニューラベル
	/// @param enabled メニューが有効かどうか
	/// @return メニューが開かれた場合にtrueを返します。
	bool BeginMainMenu(const char* label, const bool enabled) {
		ImGui::PushStyleVar(
			ImGuiStyleVar_FramePadding,
			ImVec2(
				ImGui::GetStyle().FramePadding.x,
				kTitleBarH * 0.5f - ImGui::GetFontSize() *
				0.5f
			)
		);
		ImGui::PushStyleVar(
			ImGuiStyleVar_ItemSpacing,
			ImVec2(
				ImGui::GetStyle().ItemSpacing.x,
				kTitleBarH
			)
		);
		const bool ret = ImGui::BeginMenu(label, enabled);
		ImGui::PopStyleVar(2);
		return ret;
	}

	void HandleHoveredComboMenuMouseWheelScroll(
		uint32_t& index, const uint32_t itemSize
	) {
		if (ImGui::IsItemHovered()) {
			if (itemSize == 0) {
				index = 0;
				return;
			}

			const float wheel = ImGui::GetIO().MouseWheel;
			if (wheel == 0.0f) return;

			const int delta = static_cast<int>(wheel);

			const int maxIndex = static_cast<int>(itemSize - 1);
			const int next     = std::clamp(
				static_cast<int>(index) - delta, 0, maxIndex
			);
			index = static_cast<uint32_t>(next);
		}
	}

	bool SelectableWithRounding(
		const char*   label, bool selected, const ImGuiSelectableFlags flags,
		const ImVec2& sizeArg, const float rounding
	) {
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems) return false;

		ImGuiContext&     g     = *GImGui;
		const ImGuiStyle& style = g.Style;

		// Submit label or explicit size to ItemSize(), whereas ItemAdd() will submit a larger/spanning rectangle.
		const ImGuiID id         = window->GetID(label);
		const ImVec2  label_size = ImGui::CalcTextSize(label, NULL, true);
		ImVec2        size(
			sizeArg.x != 0.0f ? sizeArg.x : label_size.x,
			sizeArg.y != 0.0f ? sizeArg.y : label_size.y
		);
		ImVec2 pos = window->DC.CursorPos;
		pos.y      += window->DC.CurrLineTextBaseOffset;
		ImGui::ItemSize(size, 0.0f);

		const bool span_all_columns =
			(flags & ImGuiSelectableFlags_SpanAllColumns) != 0;
		const float min_x = span_all_columns ?
			                    window->ParentWorkRect.Min.x :
			                    pos.x;
		const float max_x = span_all_columns ?
			                    window->ParentWorkRect.Max.x :
			                    window->WorkRect.Max.x;
		if (sizeArg.x == 0.0f || flags & ImGuiSelectableFlags_SpanAvailWidth)
			size.x = ImMax(
				label_size.x, max_x - min_x
			);

		ImRect bb(min_x, pos.y, min_x + size.x, pos.y + size.y);
		if ((flags & ImGuiSelectableFlags_NoPadWithHalfSpacing) == 0) {
			const float spacing_x = span_all_columns ?
				                        0.0f :
				                        style.ItemSpacing.x;
			const float spacing_y = style.ItemSpacing.y;
			const float spacing_L = IM_TRUNC(spacing_x * 0.50f);
			const float spacing_U = IM_TRUNC(spacing_y * 0.50f);
			bb.Min.x              -= spacing_L;
			bb.Min.y              -= spacing_U;
			bb.Max.x              += spacing_x - spacing_L;
			bb.Max.y              += spacing_y - spacing_U;
		}

		const bool disabled_item = (flags & ImGuiSelectableFlags_Disabled) != 0;
		const ImGuiItemFlags extra_item_flags = disabled_item ?
			                                        static_cast<ImGuiItemFlags>(
				                                        ImGuiItemFlags_Disabled) :
			                                        ImGuiItemFlags_None;
		bool is_visible;
		if (span_all_columns) {
			const float backup_clip_rect_min_x = window->ClipRect.Min.x;
			const float backup_clip_rect_max_x = window->ClipRect.Max.x;
			window->ClipRect.Min.x = window->ParentWorkRect.Min.x;
			window->ClipRect.Max.x = window->ParentWorkRect.Max.x;
			is_visible = ImGui::ItemAdd(bb, id, NULL, extra_item_flags);
			window->ClipRect.Min.x = backup_clip_rect_min_x;
			window->ClipRect.Max.x = backup_clip_rect_max_x;
		} else { is_visible = ImGui::ItemAdd(bb, id, NULL, extra_item_flags); }

		const bool is_multi_select =
			(g.LastItemData.ItemFlags & ImGuiItemFlags_IsMultiSelect) != 0;
		if (!is_visible)
			if (!is_multi_select || !g.BoxSelectState.UnclipMode || !g.
			    BoxSelectState.UnclipRect.Overlaps(bb))
				return false;

		const bool disabled_global =
			(g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
		if (disabled_item && !disabled_global) ImGui::BeginDisabled();

		if (span_all_columns) {
			if (g.CurrentTable) ImGui::TablePushBackgroundChannel();
			else if (window->DC.CurrentColumns) ImGui::PushColumnsBackground();
			g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasClipRect;
			g.LastItemData.ClipRect    = window->ClipRect;
		}

		ImGuiButtonFlags button_flags = 0;
		if (flags & ImGuiSelectableFlags_NoHoldingActiveID) {
			button_flags |= ImGuiButtonFlags_NoHoldingActiveId;
		}
		if (flags & ImGuiSelectableFlags_NoSetKeyOwner) {
			button_flags |= ImGuiButtonFlags_NoSetKeyOwner;
		}
		if (flags & ImGuiSelectableFlags_SelectOnClick) {
			button_flags |= ImGuiButtonFlags_PressedOnClick;
		}
		if (flags & ImGuiSelectableFlags_SelectOnRelease) {
			button_flags |= ImGuiButtonFlags_PressedOnRelease;
		}
		if (flags & ImGuiSelectableFlags_AllowDoubleClick) {
			button_flags |= ImGuiButtonFlags_PressedOnClickRelease |
				ImGuiButtonFlags_PressedOnDoubleClick;
		}
		if (flags & ImGuiSelectableFlags_AllowOverlap || g.LastItemData.
		    ItemFlags & ImGuiItemFlags_AllowOverlap) {
			button_flags |= ImGuiButtonFlags_AllowOverlap;
		}

		const bool was_selected = selected;
		if (is_multi_select) {
			ImGui::MultiSelectItemHeader(id, &selected, &button_flags);
		}

		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior(
			bb, id, &hovered, &held, button_flags
		);
		bool auto_selected = false;

		if (is_multi_select) {
			ImGui::MultiSelectItemFooter(id, &selected, &pressed);
		} else {
			if (flags & ImGuiSelectableFlags_SelectOnNav && g.NavJustMovedToId
			    != 0 && g.NavJustMovedToFocusScopeId == g.CurrentFocusScopeId)
				if (g.NavJustMovedToId == id && (
					    g.NavJustMovedToKeyMods & ImGuiMod_Ctrl) == 0)
					selected = pressed = auto_selected = true;
		}

		if (pressed || (hovered && flags &
		                ImGuiSelectableFlags_SetNavIdOnHover)) {
			if (!g.NavHighlightItemUnderNav && g.NavWindow == window && g.
			    NavLayer == window->DC.NavLayerCurrent) {
				ImGui::SetNavID(
					id, window->DC.NavLayerCurrent, g.CurrentFocusScopeId,
					ImGui::WindowRectAbsToRel(window, bb)
				);
				if (g.IO.ConfigNavCursorVisibleAuto) g.NavCursorVisible = false;
			}
		}
		if (pressed) ImGui::MarkItemEdited(id);

		if (selected !=
		    was_selected)
			g.LastItemData.StatusFlags |=
				ImGuiItemStatusFlags_ToggledSelection;

		if (is_visible) {
			const bool highlighted =
				hovered || flags & ImGuiSelectableFlags_Highlight;
			if (highlighted || selected) {
				const ImU32 col = ImGui::GetColorU32(
					held && highlighted ?
						ImGuiCol_HeaderActive :
						highlighted ?
						ImGuiCol_HeaderHovered :
						ImGuiCol_Header
				);
				ImGui::RenderFrame(bb.Min, bb.Max, col, false, rounding);
			}
			if (g.NavId == id) {
				ImGuiNavRenderCursorFlags nav_render_cursor_flags =
					ImGuiNavRenderCursorFlags_Compact |
					ImGuiNavRenderCursorFlags_NoRounding;
				if (is_multi_select)
					nav_render_cursor_flags |=
						ImGuiNavRenderCursorFlags_AlwaysDraw;
				ImGui::RenderNavCursor(bb, id, nav_render_cursor_flags);
			}
		}

		if (span_all_columns) {
			if (g.CurrentTable) ImGui::TablePopBackgroundChannel();
			else if (window->DC.CurrentColumns) ImGui::PopColumnsBackground();
		}

		if (is_visible)
			ImGui::RenderTextClipped(
				pos, ImVec2(
					ImMin(pos.x + size.x, window->WorkRect.Max.x),
					pos.y + size.y
				), label, NULL, &label_size, style.SelectableTextAlign, &bb
			);

		if (pressed && !auto_selected && window->Flags & ImGuiWindowFlags_Popup
		    && !(
			    flags & ImGuiSelectableFlags_NoAutoClosePopups) && g.
		    LastItemData.ItemFlags & ImGuiItemFlags_AutoClosePopups)
			ImGui::CloseCurrentPopup();

		if (disabled_item && !disabled_global) ImGui::EndDisabled();

		IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
		return pressed; //-V1020
	}

	/// @brief ImGui 1.92.6から持ってきました。現在のウィンドウが、開いているメニューセットのルートであるかを判定します。
	/// @return ルートである場合にtrueを返します。
	static bool IsRootOfOpenMenuSet() {
		ImGuiContext& g      = *GImGui;
		ImGuiWindow*  window = g.CurrentWindow;
		if (g.OpenPopupStack.Size <= g.BeginPopupStack.Size || window->Flags &
		    ImGuiWindowFlags_ChildMenu)
			return false;

		const ImGuiPopupData* upperPopup =
			&g.OpenPopupStack[g.BeginPopupStack.Size];
		if (window->DC.NavLayerCurrent != upperPopup->ParentNavLayer) {
			return false;
		}
		return upperPopup->Window &&
		       upperPopup->Window->Flags & ImGuiWindowFlags_ChildMenu &&
		       ImGui::IsWindowChildOf(upperPopup->Window, window, true, false);
	}

	/// @brief ImVec2同士の加算演算子
	/// @param lhs 左辺のImVec2
	/// @param rhs 右辺のImVec2
	/// @return 加算結果のImVec2
	static ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) {
		return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
	}

	bool MenuItemExWithRounding(
		const char* label, const char*   icon, const char*    shortcut,
		const bool  selected, const bool enabled, const float rounding
	) {
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems) return false;

		const ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		const ImVec2 pos = window->DC.CursorPos;
		const ImVec2 labelSize = ImGui::CalcTextSize(label, nullptr, true);

		const bool menuSetIsOpen = IsRootOfOpenMenuSet();
		if (menuSetIsOpen) {
			ImGui::PushItemFlag(ImGuiItemFlags_NoWindowHoverableCheck, true);
		}

		bool pressed;
		ImGui::PushID(label);
		if (!enabled) ImGui::BeginDisabled();

		constexpr ImGuiSelectableFlags selectableFlags =
			ImGuiSelectableFlags_SelectOnRelease |
			ImGuiSelectableFlags_NoSetKeyOwner |
			ImGuiSelectableFlags_SetNavIdOnHover;
		const ImGuiMenuColumns* offsets = &window->DC.MenuColumns;
		if (window->DC.LayoutType == ImGuiLayoutType_Horizontal) {
			const float w          = labelSize.x;
			window->DC.CursorPos.x += IM_TRUNC(style.ItemSpacing.x * 0.5f);
			const ImVec2 text_pos(
				window->DC.CursorPos.x + offsets->OffsetLabel,
				window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset
			);
			ImGui::PushStyleVarX(
				ImGuiStyleVar_ItemSpacing, style.ItemSpacing.x * 2.0f
			);
			pressed = SelectableWithRounding(
				"", selected, selectableFlags, ImVec2(w, 0.0f),
				style.FrameRounding
			);
			ImGui::PopStyleVar();
			if (g.LastItemData.StatusFlags &
			    ImGuiItemStatusFlags_Visible)
				ImGui::RenderText(text_pos, label);
			window->DC.CursorPos.x +=
				IM_TRUNC(style.ItemSpacing.x * (-1.0f + 0.5f));
		} else {
			const float iconW = icon && icon[0] ?
				                    ImGui::CalcTextSize(icon, NULL).x :
				                    0.0f;
			const float shortcutW = shortcut && shortcut[0] ?
				                        ImGui::CalcTextSize(shortcut, NULL).x :
				                        0.0f;
			const float checkmarkW = IM_TRUNC(g.FontSize * 1.20f);
			const float minW       = window->DC.MenuColumns.DeclColumns(
				iconW, labelSize.x, shortcutW, checkmarkW
			); // Feedback for next frame
			const float stretchW = ImMax(
				0.0f, ImGui::GetContentRegionAvail().x - minW
			);
			const ImVec2 textPos(
				pos.x, pos.y + window->DC.CurrLineTextBaseOffset
			);
			pressed = SelectableWithRounding(
				"", false,
				selectableFlags | ImGuiSelectableFlags_SpanAvailWidth,
				ImVec2(minW, labelSize.y),
				rounding
			);

			if (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_Visible) {
				ImGui::RenderText(
					textPos + ImVec2(offsets->OffsetLabel, 0.0f), label
				);
				if (iconW > 0.0f)
					ImGui::RenderText(
						textPos + ImVec2(offsets->OffsetIcon, 0.0f), icon
					);
				if (shortcutW > 0.0f) {
					ImGui::PushStyleColor(
						ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]
					);
					ImGui::LogSetNextTextDecoration("(", ")");
					ImGui::RenderText(
						textPos + ImVec2(
							offsets->OffsetShortcut + stretchW, 0.0f
						), shortcut, NULL, false
					);
					ImGui::PopStyleColor();
				}
				if (selected)
					ImGui::RenderCheckMark(
						window->DrawList,
						textPos + ImVec2(
							offsets->OffsetMark + stretchW + g.FontSize *
							0.40f,
							g.FontSize * 0.134f * 0.5f
						), ImGui::GetColorU32(ImGuiCol_Text),
						g.FontSize * 0.866f
					);
			}
		}
		IMGUI_TEST_ENGINE_ITEM_INFO(
			g.LastItemData.ID, label,
			g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | (
				selected ? ImGuiItemStatusFlags_Checked : 0)
		);
		if (!enabled) ImGui::EndDisabled();
		ImGui::PopID();
		if (menuSetIsOpen) ImGui::PopItemFlag();

		return pressed;
	}

	void ImageWithRounding(
		const ImTextureID textureId, const ImVec2     imageSize,
		const float       rounding, const ImDrawFlags flags,
		const ImVec2      uv0, const ImVec2           uv1,
		const ImVec4      tintColor
	) {
		const ImVec2 pos  = ImGui::GetCursorScreenPos();
		const ImVec2 size = imageSize;
		ImGui::GetWindowDrawList()->AddImageRounded(
			textureId,
			pos,
			pos + size,
			uv0, uv1,
			ImGui::GetColorU32(tintColor),
			rounding,
			flags
		);
		ImGui::Dummy(size);
	}
}
#endif
