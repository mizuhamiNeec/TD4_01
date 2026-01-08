#ifdef _DEBUG
#include <imgui_internal.h>

#include <engine/Components/Transform/SceneComponent.h>
#include <engine/Entity/Entity.h>
#include <engine/ImGui/ImGuiUtil.h>
#include <engine/ImGui/ImGuiWidgets.h>

#include <runtime/core/math/Math.h>

namespace ImGuiUtil {
	/// @brief Vec4型をImVec4型に変換します。
	/// @param vec 変換するVec4型のベクトル
	/// @return 変換後のImVec4型のベクトル
	ImVec4 ToImVec4(const Vec4& vec) {
		return {vec.x, vec.y, vec.z, vec.w};
	}

	/// @brief ImGuiのダークテーマスタイルを設定します。
	void StyleColorsDark() {
		ImGuiStyle& style  = ImGui::GetStyle();
		ImVec4*     colors = style.Colors;

		colors[ImGuiCol_WindowBg]              = {0.14f, 0.14f, 0.14f, 0.94f};
		colors[ImGuiCol_Border]                = {0.10f, 0.10f, 0.10f, 1.00f};
		colors[ImGuiCol_FrameBg]               = {0.18f, 0.18f, 0.18f, 1.00f};
		colors[ImGuiCol_FrameBgHovered]        = {0.24f, 0.24f, 0.24f, 1.00f};
		colors[ImGuiCol_FrameBgActive]         = {0.26f, 0.26f, 0.26f, 1.00f};
		colors[ImGuiCol_TitleBg]               = {0.14f, 0.14f, 0.14f, 1.00f};
		colors[ImGuiCol_TitleBgActive]         = {0.18f, 0.18f, 0.18f, 1.00f};
		colors[ImGuiCol_TitleBgCollapsed]      = {0.14f, 0.14f, 0.14f, 1.00f};
		colors[ImGuiCol_Button]                = {0.20f, 0.20f, 0.20f, 1.00f};
		colors[ImGuiCol_ButtonHovered]         = {0.30f, 0.30f, 0.30f, 1.00f};
		colors[ImGuiCol_ButtonActive]          = {0.35f, 0.35f, 0.35f, 1.00f};
		colors[ImGuiCol_Header]                = {0.23f, 0.23f, 0.23f, 1.00f};
		colors[ImGuiCol_HeaderHovered]         = {0.28f, 0.28f, 0.28f, 1.00f};
		colors[ImGuiCol_HeaderActive]          = {0.32f, 0.32f, 0.32f, 1.00f};
		colors[ImGuiCol_ResizeGrip]            = {0.30f, 0.30f, 0.30f, 1.00f};
		colors[ImGuiCol_ResizeGripHovered]     = {0.45f, 0.45f, 0.45f, 1.00f};
		colors[ImGuiCol_ResizeGripActive]      = {0.55f, 0.55f, 0.55f, 1.00f};
		colors[ImGuiCol_ScrollbarBg]           = {0.15f, 0.15f, 0.15f, 1.00f};
		colors[ImGuiCol_ScrollbarGrab]         = {0.30f, 0.30f, 0.30f, 1.00f};
		colors[ImGuiCol_ScrollbarGrabHovered]  = {0.35f, 0.35f, 0.35f, 1.00f};
		colors[ImGuiCol_ScrollbarGrabActive]   = {0.40f, 0.40f, 0.40f, 1.00f};
		colors[ImGuiCol_Text]                  = {0.71f, 0.71f, 0.71f, 1.00f};
		colors[ImGuiCol_TextDisabled]          = {0.50f, 0.50f, 0.50f, 1.00f};
		colors[ImGuiCol_CheckMark]             = {0.90f, 0.90f, 0.90f, 1.00f};
		colors[ImGuiCol_MenuBarBg]             = {0.20f, 0.20f, 0.20f, 1.00f};
		colors[ImGuiCol_SliderGrab]            = {0.89f, 0.49f, 0.02f, 1.00f};
		colors[ImGuiCol_SliderGrabActive]      = {0.89f, 0.57f, 0.19f, 1.00f};
		colors[ImGuiCol_SeparatorHovered]      = {0.89f, 0.49f, 0.02f, 0.78f};
		colors[ImGuiCol_SeparatorActive]       = {0.89f, 0.49f, 0.02f, 1.00f};
		colors[ImGuiCol_TabHovered]            = {0.20f, 0.20f, 0.20f, 0.81f};
		colors[ImGuiCol_Tab]                   = {0.25f, 0.25f, 0.25f, 0.86f};
		colors[ImGuiCol_TabSelected]           = {0.31f, 0.31f, 0.31f, 1.00f};
		colors[ImGuiCol_TabSelectedOverline]   = {0.89f, 0.49f, 0.02f, 1.00f};
		colors[ImGuiCol_TabDimmed]             = {0.18f, 0.18f, 0.18f, 0.97f};
		colors[ImGuiCol_TabDimmedSelected]     = {0.25f, 0.25f, 0.25f, 1.00f};
		colors[ImGuiCol_DockingPreview]        = {0.89f, 0.49f, 0.02f, 0.70f};
		colors[ImGuiCol_TextLink]              = {0.89f, 0.49f, 0.02f, 1.00f};
		colors[ImGuiCol_TextSelectedBg]        = {0.89f, 0.49f, 0.02f, 0.70f};
		colors[ImGuiCol_NavCursor]             = {0.89f, 0.49f, 0.02f, 1.00f};
		colors[ImGuiCol_NavWindowingHighlight] = {0.89f, 0.49f, 0.02f, 0.70f};
		colors[ImGuiCol_NavWindowingDimBg]     = {0.17f, 0.17f, 0.17f, 0.86f};
		colors[ImGuiCol_ModalWindowDimBg]      = {0.17f, 0.17f, 0.17f, 0.86f};

		// Main
		style.WindowPadding    = {4, 4};
		style.FramePadding     = {4, 4};
		style.ItemSpacing      = {6, 6};
		style.ItemInnerSpacing = {2, 2};
		style.IndentSpacing    = 20.0f;
		style.GrabMinSize      = 4.0f;

		// Borders
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize  = 1.0f;
		style.PopupBorderSize  = 1.0f;
		style.FrameBorderSize  = 0.0f;

		// Rounding
		style.WindowRounding = 4.0f;
		style.ChildRounding  = 2.0f;
		style.FrameRounding  = 4.0f;
		style.PopupRounding  = 8.0f;
		style.GrabRounding   = 12.0f;

		// Scrollbar
		style.ScrollbarSize     = 14.0f;
		style.ScrollbarRounding = 8.0f;
		style.ScrollbarPadding  = 0.0f;

		// Tabs
		style.TabBorderSize      = 1.0f;
		style.TabBarBorderSize   = 1.0f;
		style.TabBarOverlineSize = 2.0f;
		style.TabMinWidthBase    = 1.0f;
		style.TabMinWidthShrink  = 80.0f;
		style.TabRounding        = 4.0f;

		// Tables
		style.CellPadding             = ImVec2(2, 2);
		style.TableAngledHeadersAngle = 35.0f;

		// Trees
		style.TreeLinesFlags    = ImGuiTreeNodeFlags_DrawLinesToNodes;
		style.TreeLinesSize     = 1.0f;
		style.TreeLinesRounding = 0.0f;

		// Windows
		style.WindowTitleAlign         = {0.5f, 0.5f};
		style.WindowBorderHoverPadding = 4.0f;
		style.WindowMenuButtonPosition = ImGuiDir_Left;

		// Widgets
		style.ColorButtonPosition     = ImGuiDir_Right;
		style.ButtonTextAlign         = {0.5f, 0.5f};
		style.SelectableTextAlign     = {0.0f, 0.0f};
		style.SeparatorTextBorderSize = 2.0f;
		style.ImageBorderSize         = 0.0f;

		// Docking
		style.DockingSeparatorSize = 3.0f;

		// Tooltips
		style.HoverFlagsForTooltipMouse = ImGuiHoveredFlags_DelayNone; // 絶対いらん
	}

	/// @brief ImGuiのライトテーマスタイルを設定します。
	void StyleColorsLight() {
		ImGui::StyleColorsLight();
	}

	///	@brief Drag用のスタイルカラーをプッシュします。
	/// @param bg 通常時の背景色
	/// @param bgHovered ホバー時の背景色
	/// @param bgActive アクティブ時の背景色
	void PushStyleColorForDrag(
		const ImVec4& bg, const ImVec4& bgHovered, const ImVec4& bgActive
	) {
		ImGui::PushStyleColor(ImGuiCol_FrameBg, bg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bgHovered);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, bgActive);
	}

	/// @brief Transformコンポーネントの編集ウィジェットを表示します。
	/// @param transform 編集するTransformコンポーネントへの参照
	/// @param vSpeed ドラッグ操作の速度
	bool EditTransform(
		SceneComponent& transform, float vSpeed
	) {
		bool       isEditing  = false;
		Vec3       localPos   = transform.GetLocalPos();
		Quaternion localRot   = transform.GetLocalRot();
		Vec3       localScale = transform.GetLocalScale();

		if (ImGui::CollapsingHeader(
			("Transform##" + transform.GetOwner()->GetName()).c_str(),
			ImGuiTreeNodeFlags_DefaultOpen)) {
			// Position 編集
			if (ImGuiWidgets::DragVec3(
				"Position",
				localPos,
				Vec3::zero,
				vSpeed,
				"%.3f"
			)) {
				transform.SetLocalPos(localPos);
				isEditing = true;
			}

			// Rotation 編集
			Vec3 eulerDegrees = localRot.ToEulerDegrees();
			if (ImGuiWidgets::DragVec3(
					"Rotation",
					eulerDegrees,
					Vec3::zero,
					vSpeed * 10.0f,
					"%.3f")
			) {
				// 編集された Euler 角を Quaternion に変換
				localRot = Quaternion::EulerDegrees(eulerDegrees);
				transform.SetLocalRot(localRot);
				isEditing = true;
			}

			// Scale 編集
			if (ImGuiWidgets::DragVec3(
					"Scale",
					localScale,
					Vec3::one,
					vSpeed,
					"%.3f")
			) {
				transform.SetLocalScale(localScale);
				isEditing = true;
			}
		}
		return isEditing;
	}

	/// @brief Vec3型の値をドラッグで編集するウィジェットを表示します。
	/// @param name ウィジェットの名前
	/// @param v 編集するVec3型の参照
	/// @param vSpeed ドラッグ操作の速度
	/// @param format 表示フォーマット
	/// @return 編集された場合はtrueを返す
	bool DragVec3(
		const std::string& name, Vec3& v, float vSpeed,
		const char*        format
	) {
		// 編集中かどうか
		bool isEditing = false;

		// XYZの3つ分
		constexpr int components = 3;

		// 幅を取得
		const float width = ImGui::GetCurrentContext()->Style.ItemInnerSpacing.
			x;

		constexpr ImVec4 xBg        = {0.72f, 0.11f, 0.11f, 0.75f};
		constexpr ImVec4 xBgHovered = {0.83f, 0.18f, 0.18f, 0.75f};
		constexpr ImVec4 xBgActive  = {0.96f, 0.26f, 0.21f, 0.75f};

		constexpr ImVec4 yBg        = {0.11f, 0.37f, 0.13f, 0.75f};
		constexpr ImVec4 yBgHovered = {0.22f, 0.56f, 0.24f, 0.75f};
		constexpr ImVec4 yBgActive  = {0.30f, 0.69f, 0.31f, 0.75f};

		constexpr ImVec4 zBg        = {0.05f, 0.28f, 0.63f, 0.75f};
		constexpr ImVec4 zBgHovered = {0.10f, 0.46f, 0.82f, 0.75f};
		constexpr ImVec4 zBgActive  = {0.13f, 0.59f, 0.95f, 0.75f};

		// 幅を決定
		ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());

		/* --- 座標 --- */
		// 色を送る
		ImGui::PushID("X");
		PushStyleColorForDrag(xBg, xBgHovered, xBgActive);
		isEditing |= ImGui::DragFloat(("##X" + name).c_str(), &v.x, vSpeed,
		                              0.0f,
		                              0.0f, format);
		ImGui::PopStyleColor(components);
		ImGui::PopID();
		ImGui::SameLine(0, width);

		ImGui::PushID("Y");
		PushStyleColorForDrag(yBg, yBgHovered, yBgActive);
		isEditing |= ImGui::DragFloat(("##Y" + name).c_str(), &v.y, vSpeed,
		                              0.0f,
		                              0.0f, format);
		ImGui::PopStyleColor(components);
		ImGui::PopID();
		ImGui::SameLine(0, width);

		ImGui::PushID("Z");
		PushStyleColorForDrag(zBg, zBgHovered, zBgActive);
		isEditing |= ImGui::DragFloat(("##Z" + name).c_str(), &v.z, vSpeed,
		                              0.0f,
		                              0.0f, format);
		ImGui::PopStyleColor(components);
		ImGui::PopID();
		ImGui::SameLine(0, width);

		ImGui::Text(name.c_str());

		for (int i = 0; i < components; i++) {
			ImGui::PopItemWidth();
		}

		return isEditing;
	}

	/// @brief アウトライン付きのテキストを描画します。
	/// @param drawList 描画リストへのポインタ
	/// @param pos テキストの位置
	/// @param text 描画するテキスト
	/// @param textColor テキストの色
	/// @param outlineColor アウトラインの
	/// @param outlineSize アウトラインのサイズ
	void TextOutlined(
		ImDrawList*   drawList,
		const ImVec2& pos,
		const char*   text,
		const ImVec4  textColor,
		const ImVec4  outlineColor,
		const float   outlineSize
	) {
		// クライアント領域の左上座標を取得
		const auto windowPos = ImGui::GetWindowPos();
		const auto clientPos = ImVec2(windowPos.x + pos.x, windowPos.y + pos.y);

		const auto outlineCol = ImGui::ColorConvertFloat4ToU32(outlineColor);
		const auto textCol    = ImGui::ColorConvertFloat4ToU32(textColor);

		drawList->AddText(
			{clientPos.x - outlineSize, clientPos.y}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x + outlineSize, clientPos.y}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x, clientPos.y - outlineSize}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x, clientPos.y + outlineSize}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x - outlineSize, clientPos.y - outlineSize}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x + outlineSize, clientPos.y - outlineSize}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x - outlineSize, clientPos.y + outlineSize}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x + outlineSize, clientPos.y + outlineSize}, outlineCol,
			text
		);
		drawList->AddText(clientPos, textCol, text);
	}

	bool CollapsingHeaderWithCheckbox(
		const char* label, bool* v, const ImGuiTreeNodeFlags flags
	) {
		ImGui::PushID(label);
		const bool isOpen = ImGui::CollapsingHeader(
			"##CollapsingHeader", flags | ImGuiTreeNodeFlags_AllowOverlap
		);
		ImGui::SameLine();
		ImGui::Checkbox("##Checkbox", v);
		ImGui::SameLine();
		ImGui::Text(label);
		ImGui::PopID();
		return isOpen;
	}
}
#endif
