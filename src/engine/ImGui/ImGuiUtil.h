#pragma once

#ifdef _DEBUG
#include <imgui.h>
#include <string>
struct Vec3;
class SceneComponent;
struct Vec4;

namespace ImGuiUtil {
#ifdef _DEBUG
	ImVec4 ToImVec4(const Vec4& vec);

	void StyleColorsDark();
	void StyleColorsLight();

	void PushStyleColorForDrag(
		const ImVec4& bg, const ImVec4& bgHovered,
		const ImVec4& bgActive
	);
	bool EditTransform(
		SceneComponent& transform,
		float           vSpeed
	);
	bool DragVec3(
		const std::string& name, Vec3& v, float vSpeed,
		const char*        format
	);
	void TextOutlined(
		ImDrawList* drawList, const ImVec2& pos, const char* text,
		ImVec4      textColor, ImVec4       outlineColor,
		float       outlineSize = 1.0f
	);

	bool CollapsingHeaderWithCheckbox(
		const char* label, bool* v, ImGuiTreeNodeFlags flags = 0
	);
#endif
}
#endif
