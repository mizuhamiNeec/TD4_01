#pragma once
#include <string>

#ifdef _DEBUG
#include <imgui.h>

struct Vec3;

namespace ImGuiWidgets {
	/// @brief Dragウィジェット用のスタイルカラーをプッシュします。
	/// @param bg 通常時の背景色
	/// @param bgHovered ホバー時の背景色
	/// @param bgActive アクティブ時の背景色
	void PushStyleColorForDrag(
		const ImVec4& bg,
		const ImVec4& bgHovered,
		const ImVec4& bgActive
	);

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
		Vec3               defaultValue,
		float              vSpeed,
		const char*        format
	);

	/// @brief Cubic Bézier曲線の編集ウィジェットを表示します。
	/// @param label ウィジェットのラベル
	/// @param p0 コントロールポイント1のX座標への参照
	/// @param p1 コントロールポイント1のY座標への参照
	/// @param p2 コントロールポイント2のX座標への参照
	/// @param p3 コントロールポイント2のY座標への参照
	/// @return 値が変更された場合にtrueを返します。
	bool EditCubicBezier(
		const std::string& label,
		float              p0,
		float              p1,
		float              p2,
		float              p3
	);

	/// @brief アイコン付きボタンウィジェットを表示します。
	/// @param icon アイコン文字列（フォントアイコン）
	/// @param label ラベル文字列（省略可能）
	/// @param size ボタンのサイズ（(0,0)で自動調整）
	/// @param iconScale アイコンのスケーリング（高さに対する比率）
	/// @param labelDir ラベルの配置方向
	/// @return ボタンが押された場合にtrueを返します。
	bool IconButton(
		const char* icon,
		const char* label     = nullptr,
		ImVec2      size      = ImVec2(0, 0),
		float       iconScale = 1.0f,
		ImGuiDir    labelDir  = ImGuiDir_Down
	);

	/// @brief アイコン付きメニューアイテムを表示します。
	/// @param icon アイコン文字列（フォントアイコン）
	/// @param label ラベル文字列
	/// @param shortcut ショートカット文字列（省略可能）
	/// @param selected 選択状態かどうか
	/// @param enabled メニューアイテムが有効かどうか
	/// @return メニューアイテムが選択された場合にtrueを返します
	bool MenuItemWithIcon(
		const char* label,
		uint32_t    icon,
		const char* shortcut = nullptr,
		bool        selected = false,
		bool        enabled  = true
	);

	/// @brief メインメニューバーを開始します。
	/// @param label メニューバーのラベル
	/// @param enabled メニューバーが有効かどうか
	/// @return メニューバーが開始された場合にtrueを返します
	bool BeginMainMenu(const char* label, bool enabled = true);

	/// @brief ホバー中のコンボメニューに対するマウスホイールスクロールを処理します。
	/// @param index 現在の選択インデックスへの参照
	/// @param itemSize コンボメニューの項目数
	void HandleHoveredComboMenuMouseWheelScroll(
		uint32_t& index, uint32_t itemSize
	);
}
#endif
