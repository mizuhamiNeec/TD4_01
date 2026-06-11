#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "UiComponent.h"

#include "engine/gui/UiDrawCommand.h"

namespace Unnamed {
	template <typename T>
	class ConVar;
}

namespace Unnamed::Gui {
	/// @brief float ConVar をスライダーとして編集する UI Behavior です。
	class UiConVarFloatSliderBehaviorComponent final : public UiComponent {
	public:
		[[nodiscard]] std::string_view GetTypeName() const override {
			return "ConVarFloatSliderBehavior";
		}

		void BuildDrawCommands(
			const UiWidget& owner, std::vector<UiDrawCommand>& out
		) const override;
		void OnMouseDown(UiWidget& owner) override;
		void OnMouseDrag(UiWidget& owner) override;
		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

		/// @brief 操作対象の ConVar 名を設定します。
		void SetConVarName(std::string_view name);
		/// @brief 操作対象の ConVar 名を取得します。
		[[nodiscard]] std::string_view GetConVarName() const;
		/// @brief スライダーの値範囲を設定します。
		void SetRange(float minValue, float maxValue);
		/// @brief スライダーの最小値を取得します。
		[[nodiscard]] float GetMinValue() const;
		/// @brief スライダーの最大値を取得します。
		[[nodiscard]] float GetMaxValue() const;
		/// @brief トラックの表示高さを設定します。
		void SetTrackHeight(float height);
		/// @brief トラックの表示高さを取得します。
		[[nodiscard]] float GetTrackHeight() const;
		/// @brief ノブの表示サイズを設定します。
		void SetKnobSize(float width, float height);
		/// @brief ノブの表示幅を取得します。
		[[nodiscard]] float GetKnobWidth() const;
		/// @brief ノブの表示高さを取得します。
		[[nodiscard]] float GetKnobHeight() const;
		/// @brief 値の丸め幅を設定します。0 の場合は丸めません。
		void SetStep(float step);
		/// @brief 値の丸め幅を取得します。
		[[nodiscard]] float GetStep() const;
		/// @brief スライダー描画色を設定します。
		void SetColors(
			const Color& normal,
			const Color& fill,
			const Color& knob
		);
		/// @brief トラックの通常色を取得します。
		[[nodiscard]] const Color& GetNormalColor() const;
		/// @brief トラックの塗りつぶし色を取得します。
		[[nodiscard]] const Color& GetFillColor() const;
		/// @brief ノブの色を取得します。
		[[nodiscard]] const Color& GetKnobColor() const;
		/// @brief 値変更時に実行するコンソールコマンド配列を取得します。
		[[nodiscard]] std::vector<std::string>& GetOnChangedCommands();

	private:
		[[nodiscard]] ConVar<float>* ResolveConVar() const;
		[[nodiscard]] bool           IsRangeValid() const;
		void                         ApplyMousePosition(const UiWidget& owner);
		void                         RunChangedCommands() const;
		void                         WarnOnce(const std::string& message) const;

		std::string mConVarName;
		float       mMinValue   = 0.0f;
		float       mMaxValue   = 1.0f;
		float       mTrackHeight = 10.0f;
		float       mKnobWidth   = 18.0f;
		float       mKnobHeight  = 28.0f;
		float       mStep        = 0.0f;

		Color mNormalColor = {.r = 0.25f, .g = 0.25f, .b = 0.30f, .a = 1.0f};
		Color mFillColor   = {.r = 0.60f, .g = 0.60f, .b = 0.70f, .a = 1.0f};
		Color mKnobColor   = {.r = 1.00f, .g = 1.00f, .b = 1.00f, .a = 1.0f};

		std::vector<std::string> mOnChangedCommands;
		mutable bool             mWarned = false;
	};

	/// @brief bool ConVar をチェックボックスとして編集する UI Behavior です。
	class UiConVarBoolCheckboxBehaviorComponent final : public UiComponent {
	public:
		[[nodiscard]] std::string_view GetTypeName() const override {
			return "ConVarBoolCheckboxBehavior";
		}

		void BuildDrawCommands(
			const UiWidget& owner, std::vector<UiDrawCommand>& out
		) const override;
		void OnClick(UiWidget& owner) override;
		void Serialize(JsonWriter& writer) const override;
		void Deserialize(const JsonReader& reader) override;

		/// @brief 操作対象の ConVar 名を設定します。
		void SetConVarName(std::string_view name);
		/// @brief 操作対象の ConVar 名を取得します。
		[[nodiscard]] std::string_view GetConVarName() const;
		/// @brief チェックボックス描画色を設定します。
		void SetColors(
			const Color& normal,
			const Color& checked,
			const Color& hovered,
			const Color& pressed
		);
		/// @brief 未チェック時の通常色を取得します。
		[[nodiscard]] const Color& GetNormalColor() const;
		/// @brief チェック時の色を取得します。
		[[nodiscard]] const Color& GetCheckedColor() const;
		/// @brief ホバー時の色を取得します。
		[[nodiscard]] const Color& GetHoveredColor() const;
		/// @brief 押下時の色を取得します。
		[[nodiscard]] const Color& GetPressedColor() const;
		/// @brief 値変更時に実行するコンソールコマンド配列を取得します。
		[[nodiscard]] std::vector<std::string>& GetOnChangedCommands();

	private:
		[[nodiscard]] ConVar<bool>* ResolveConVar() const;
		void                        RunChangedCommands() const;
		void                        WarnOnce(const std::string& message) const;

		std::string mConVarName;

		Color mNormalColor  = {.r = 0.25f, .g = 0.25f, .b = 0.30f, .a = 1.0f};
		Color mCheckedColor = {.r = 0.60f, .g = 0.80f, .b = 0.60f, .a = 1.0f};
		Color mHoveredColor = {.r = 0.32f, .g = 0.32f, .b = 0.38f, .a = 1.0f};
		Color mPressedColor = {.r = 0.18f, .g = 0.18f, .b = 0.25f, .a = 1.0f};

		std::vector<std::string> mOnChangedCommands;
		mutable bool             mWarned = false;
	};
}
