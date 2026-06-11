#include "UiConVarBehaviorComponents.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/gui/UiWidget.h"
#include "engine/unnamed/subsystem/console/ConsoleSystem.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/console/concommand/ConVar.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed::Gui {
	namespace {
		static constexpr std::string_view kFloatChannel =
			"ConVarFloatSliderBehavior";
		static constexpr std::string_view kBoolChannel =
			"ConVarBoolCheckboxBehavior";

		void WriteColor(JsonWriter& writer, const Color& color) {
			writer.BeginArray();
			writer.Write(color.r);
			writer.Write(color.g);
			writer.Write(color.b);
			writer.Write(color.a);
			writer.EndArray();
		}

		Color ReadColor(const JsonReader& reader, const Color& fallback) {
			if (!reader.Valid() || reader.Size() < 4) {
				return fallback;
			}
			return {
				.r = reader[0].GetFloat(),
				.g = reader[1].GetFloat(),
				.b = reader[2].GetFloat(),
				.a = reader[3].GetFloat(),
			};
		}

		void ReadCommands(
			const JsonReader& reader,
			std::vector<std::string>& outCommands
		) {
			if (!reader.Valid() || !reader.IsArray()) {
				return;
			}
			outCommands.clear();
			for (size_t i = 0; i < reader.Size(); ++i) {
				outCommands.emplace_back(reader[i].GetString(""));
			}
		}

		void WriteCommands(
			JsonWriter& writer,
			const std::vector<std::string>& commands
		) {
			writer.BeginArray();
			for (const auto& command : commands) {
				writer.Write(command);
			}
			writer.EndArray();
		}

		void ExecuteCommands(
			const std::string_view channel,
			const std::vector<std::string>& commands
		) {
			if (commands.empty()) {
				return;
			}

			ConsoleSystem* console = ServiceLocator::Get<ConsoleSystem>();
			if (!console) {
				Warning(
					channel,
					"onChangedCommands skipped: ConsoleSystem is not available."
				);
				return;
			}

			for (const auto& command : commands) {
				if (!command.empty()) {
					console->ExecuteCommand(command, EXEC_FLAG::FROM_ENGINE);
				}
			}
		}

		void PushRect(
			std::vector<UiDrawCommand>& out,
			const Rect&                 rect,
			const Color&                color,
			const float                 cornerRadius = 0.0f
		) {
			if (rect.width <= 0.0f || rect.height <= 0.0f) {
				return;
			}

			UiDrawCommand command     = {};
			command.type              = UI_DRAW_COMMAND_TYPE::RECT;
			command.rect.rect         = rect;
			command.rect.fillColor    = color;
			command.rect.cornerRadius = cornerRadius;
			out.emplace_back(command);
		}

		template <typename T>
		ConVar<T>* ResolveTypedConVar(
			const std::string_view channel,
			const std::string&     conVarName,
			bool&                  warned
		) {
			if (conVarName.empty()) {
				if (!warned) {
					Warning(channel, "conVarName is empty.");
					warned = true;
				}
				return nullptr;
			}

			ConsoleSystem* console = ServiceLocator::Get<ConsoleSystem>();
			if (!console) {
				if (!warned) {
					Warning(channel, "ConsoleSystem is not available.");
					warned = true;
				}
				return nullptr;
			}

			ConCommandBase* base = console->GetConVar(conVarName);
			if (!base) {
				if (!warned) {
					Warning(channel, "ConVar '{}' was not found.", conVarName);
					warned = true;
				}
				return nullptr;
			}

			auto* typed = dynamic_cast<ConVar<T>*>(base);
			if (!typed) {
				if (!warned) {
					Warning(
						channel,
						"ConVar '{}' has an incompatible type.",
						conVarName
					);
					warned = true;
				}
				return nullptr;
			}

			return typed;
		}
	}

	void UiConVarFloatSliderBehaviorComponent::BuildDrawCommands(
		const UiWidget&             owner,
		std::vector<UiDrawCommand>& out
	) const {
		if (!owner.IsVisible()) {
			return;
		}

		const Rect rect = owner.GetGlobalRect();
		if (rect.width <= 0.0f || rect.height <= 0.0f) {
			return;
		}

		const float trackHeight = std::clamp(
			mTrackHeight,
			1.0f,
			std::max(1.0f, rect.height)
		);
		const Rect trackRect = {
			.x      = rect.x,
			.y      = rect.y + (rect.height - trackHeight) * 0.5f,
			.width  = rect.width,
			.height = trackHeight,
		};
		PushRect(out, trackRect, mNormalColor, trackHeight * 0.5f);

		const ConVar<float>* conVar = ResolveConVar();
		if (!conVar || !IsRangeValid()) {
			return;
		}

		const float clampedValue = std::clamp(
			conVar->GetValue(),
			mMinValue,
			mMaxValue
		);
		const float t = (clampedValue - mMinValue) / (mMaxValue - mMinValue);
		const float fillWidth = rect.width * std::clamp(t, 0.0f, 1.0f);
		PushRect(
			out,
			{
				.x      = trackRect.x,
				.y      = trackRect.y,
				.width  = fillWidth,
				.height = trackRect.height,
			},
			mFillColor,
			trackHeight * 0.5f
		);

		const float knobWidth = std::clamp(
			mKnobWidth,
			1.0f,
			std::max(1.0f, rect.width)
		);
		const float knobHeight = std::clamp(
			mKnobHeight,
			1.0f,
			std::max(1.0f, rect.height)
		);
		const float knobCenterX = rect.x + fillWidth;
		PushRect(
			out,
			{
				.x      = knobCenterX - knobWidth * 0.5f,
				.y      = rect.y + (rect.height - knobHeight) * 0.5f,
				.width  = knobWidth,
				.height = knobHeight,
			},
			mKnobColor,
			4.0f
		);
	}

	void UiConVarFloatSliderBehaviorComponent::OnMouseDown(UiWidget& owner) {
		ApplyMousePosition(owner);
	}

	void UiConVarFloatSliderBehaviorComponent::OnMouseDrag(UiWidget& owner) {
		ApplyMousePosition(owner);
	}

	void UiConVarFloatSliderBehaviorComponent::Serialize(
		JsonWriter& writer
	) const {
		writer.Key("conVarName");
		writer.Write(mConVarName);
		writer.Key("minValue");
		writer.Write(mMinValue);
		writer.Key("maxValue");
		writer.Write(mMaxValue);
		writer.Key("trackHeight");
		writer.Write(mTrackHeight);
		writer.Key("knobWidth");
		writer.Write(mKnobWidth);
		writer.Key("knobHeight");
		writer.Write(mKnobHeight);
		writer.Key("step");
		writer.Write(mStep);
		writer.Key("normalColor");
		WriteColor(writer, mNormalColor);
		writer.Key("fillColor");
		WriteColor(writer, mFillColor);
		writer.Key("knobColor");
		WriteColor(writer, mKnobColor);
		writer.Key("onChangedCommands");
		WriteCommands(writer, mOnChangedCommands);
	}

	void UiConVarFloatSliderBehaviorComponent::Deserialize(
		const JsonReader& reader
	) {
		mWarned = false;
		if (reader.Has("conVarName")) {
			mConVarName = reader["conVarName"].GetString("");
		}
		if (reader.Has("minValue")) {
			mMinValue = reader["minValue"].GetFloat(mMinValue);
		}
		if (reader.Has("maxValue")) {
			mMaxValue = reader["maxValue"].GetFloat(mMaxValue);
		}
		if (reader.Has("trackHeight")) {
			mTrackHeight = reader["trackHeight"].GetFloat(mTrackHeight);
		}
		if (reader.Has("knobWidth")) {
			mKnobWidth = reader["knobWidth"].GetFloat(mKnobWidth);
		}
		if (reader.Has("knobHeight")) {
			mKnobHeight = reader["knobHeight"].GetFloat(mKnobHeight);
		}
		if (reader.Has("step")) {
			mStep = std::max(0.0f, reader["step"].GetFloat(mStep));
		}
		if (reader.Has("normalColor")) {
			mNormalColor = ReadColor(reader["normalColor"], mNormalColor);
		}
		if (reader.Has("fillColor")) {
			mFillColor = ReadColor(reader["fillColor"], mFillColor);
		}
		if (reader.Has("knobColor")) {
			mKnobColor = ReadColor(reader["knobColor"], mKnobColor);
		}
		if (reader.Has("onChangedCommands")) {
			ReadCommands(reader["onChangedCommands"], mOnChangedCommands);
		}
	}

	void UiConVarFloatSliderBehaviorComponent::SetConVarName(
		const std::string_view name
	) {
		mConVarName = name;
		mWarned     = false;
	}

	std::string_view UiConVarFloatSliderBehaviorComponent::GetConVarName()
		const {
		return mConVarName;
	}

	void UiConVarFloatSliderBehaviorComponent::SetRange(
		const float minValue,
		const float maxValue
	) {
		mMinValue = minValue;
		mMaxValue = maxValue;
		mWarned   = false;
	}

	float UiConVarFloatSliderBehaviorComponent::GetMinValue() const {
		return mMinValue;
	}

	float UiConVarFloatSliderBehaviorComponent::GetMaxValue() const {
		return mMaxValue;
	}

	void UiConVarFloatSliderBehaviorComponent::SetTrackHeight(
		const float height
	) {
		mTrackHeight = std::max(0.0f, height);
	}

	float UiConVarFloatSliderBehaviorComponent::GetTrackHeight() const {
		return mTrackHeight;
	}

	void UiConVarFloatSliderBehaviorComponent::SetKnobSize(
		const float width,
		const float height
	) {
		mKnobWidth  = std::max(0.0f, width);
		mKnobHeight = std::max(0.0f, height);
	}

	float UiConVarFloatSliderBehaviorComponent::GetKnobWidth() const {
		return mKnobWidth;
	}

	float UiConVarFloatSliderBehaviorComponent::GetKnobHeight() const {
		return mKnobHeight;
	}

	void UiConVarFloatSliderBehaviorComponent::SetStep(const float step) {
		mStep = std::max(0.0f, step);
	}

	float UiConVarFloatSliderBehaviorComponent::GetStep() const {
		return mStep;
	}

	void UiConVarFloatSliderBehaviorComponent::SetColors(
		const Color& normal,
		const Color& fill,
		const Color& knob
	) {
		mNormalColor = normal;
		mFillColor   = fill;
		mKnobColor   = knob;
	}

	const Color& UiConVarFloatSliderBehaviorComponent::GetNormalColor() const {
		return mNormalColor;
	}

	const Color& UiConVarFloatSliderBehaviorComponent::GetFillColor() const {
		return mFillColor;
	}

	const Color& UiConVarFloatSliderBehaviorComponent::GetKnobColor() const {
		return mKnobColor;
	}

	std::vector<std::string>&
	UiConVarFloatSliderBehaviorComponent::GetOnChangedCommands() {
		return mOnChangedCommands;
	}

	ConVar<float>* UiConVarFloatSliderBehaviorComponent::ResolveConVar() const {
		return ResolveTypedConVar<float>(
			kFloatChannel,
			mConVarName,
			mWarned
		);
	}

	bool UiConVarFloatSliderBehaviorComponent::IsRangeValid() const {
		if (mMinValue < mMaxValue) {
			return true;
		}
		WarnOnce(
			std::format(
				"ConVar '{}' has invalid slider range: minValue ({}) >= maxValue ({}).",
				mConVarName,
				mMinValue,
				mMaxValue
			)
		);
		return false;
	}

	void UiConVarFloatSliderBehaviorComponent::ApplyMousePosition(
		const UiWidget& owner
	) {
		auto* conVar = ResolveConVar();
		if (!conVar || !IsRangeValid()) {
			return;
		}

		const Rect rect = owner.GetGlobalRect();
		if (rect.width <= 0.0f) {
			return;
		}

		const Vec2  mouse = owner.GetMousePosition();
		const float t     = std::clamp((mouse.x - rect.x) / rect.width, 0.0f, 1.0f);
		float       value = mMinValue + (mMaxValue - mMinValue) * t;
		if (mStep > 0.0f) {
			value = mMinValue + std::round((value - mMinValue) / mStep) * mStep;
		}
		value = std::clamp(value, mMinValue, mMaxValue);
		if (conVar->GetValue() != value) {
			conVar->SetValue(value);
			RunChangedCommands();
		}
	}

	void UiConVarFloatSliderBehaviorComponent::RunChangedCommands() const {
		ExecuteCommands(kFloatChannel, mOnChangedCommands);
	}

	void UiConVarFloatSliderBehaviorComponent::WarnOnce(
		const std::string& message
	) const {
		if (mWarned) {
			return;
		}
		Warning(kFloatChannel, "{}", message);
		mWarned = true;
	}

	void UiConVarBoolCheckboxBehaviorComponent::BuildDrawCommands(
		const UiWidget&             owner,
		std::vector<UiDrawCommand>& out
	) const {
		if (!owner.IsVisible()) {
			return;
		}

		const Rect rect = owner.GetGlobalRect();
		if (rect.width <= 0.0f || rect.height <= 0.0f) {
			return;
		}

		const ConVar<bool>* conVar = ResolveConVar();
		const bool          checked = conVar ? conVar->GetValue() : false;
		Color               color   = checked ? mCheckedColor : mNormalColor;
		if (owner.IsPressed()) {
			color = mPressedColor;
		} else if (owner.IsHovered() && !checked) {
			color = mHoveredColor;
		}
		PushRect(out, rect, color, 4.0f);

		if (checked) {
			const float inset = std::min(rect.width, rect.height) * 0.24f;
			PushRect(
				out,
				{
					.x      = rect.x + inset,
					.y      = rect.y + inset,
					.width  = std::max(0.0f, rect.width - inset * 2.0f),
					.height = std::max(0.0f, rect.height - inset * 2.0f),
				},
				{.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f},
				2.0f
			);
		}
	}

	void UiConVarBoolCheckboxBehaviorComponent::OnClick(UiWidget& owner) {
		(void)owner;
		auto* conVar = ResolveConVar();
		if (!conVar) {
			return;
		}
		conVar->SetValue(!conVar->GetValue());
		RunChangedCommands();
	}

	void UiConVarBoolCheckboxBehaviorComponent::Serialize(
		JsonWriter& writer
	) const {
		writer.Key("conVarName");
		writer.Write(mConVarName);
		writer.Key("normalColor");
		WriteColor(writer, mNormalColor);
		writer.Key("checkedColor");
		WriteColor(writer, mCheckedColor);
		writer.Key("hoveredColor");
		WriteColor(writer, mHoveredColor);
		writer.Key("pressedColor");
		WriteColor(writer, mPressedColor);
		writer.Key("onChangedCommands");
		WriteCommands(writer, mOnChangedCommands);
	}

	void UiConVarBoolCheckboxBehaviorComponent::Deserialize(
		const JsonReader& reader
	) {
		mWarned = false;
		if (reader.Has("conVarName")) {
			mConVarName = reader["conVarName"].GetString("");
		}
		if (reader.Has("normalColor")) {
			mNormalColor = ReadColor(reader["normalColor"], mNormalColor);
		}
		if (reader.Has("checkedColor")) {
			mCheckedColor = ReadColor(reader["checkedColor"], mCheckedColor);
		}
		if (reader.Has("hoveredColor")) {
			mHoveredColor = ReadColor(reader["hoveredColor"], mHoveredColor);
		}
		if (reader.Has("pressedColor")) {
			mPressedColor = ReadColor(reader["pressedColor"], mPressedColor);
		}
		if (reader.Has("onChangedCommands")) {
			ReadCommands(reader["onChangedCommands"], mOnChangedCommands);
		}
	}

	void UiConVarBoolCheckboxBehaviorComponent::SetConVarName(
		const std::string_view name
	) {
		mConVarName = name;
		mWarned     = false;
	}

	std::string_view UiConVarBoolCheckboxBehaviorComponent::GetConVarName()
		const {
		return mConVarName;
	}

	void UiConVarBoolCheckboxBehaviorComponent::SetColors(
		const Color& normal,
		const Color& checked,
		const Color& hovered,
		const Color& pressed
	) {
		mNormalColor  = normal;
		mCheckedColor = checked;
		mHoveredColor = hovered;
		mPressedColor = pressed;
	}

	const Color& UiConVarBoolCheckboxBehaviorComponent::GetNormalColor() const {
		return mNormalColor;
	}

	const Color& UiConVarBoolCheckboxBehaviorComponent::GetCheckedColor() const {
		return mCheckedColor;
	}

	const Color& UiConVarBoolCheckboxBehaviorComponent::GetHoveredColor() const {
		return mHoveredColor;
	}

	const Color& UiConVarBoolCheckboxBehaviorComponent::GetPressedColor() const {
		return mPressedColor;
	}

	std::vector<std::string>&
	UiConVarBoolCheckboxBehaviorComponent::GetOnChangedCommands() {
		return mOnChangedCommands;
	}

	ConVar<bool>* UiConVarBoolCheckboxBehaviorComponent::ResolveConVar() const {
		return ResolveTypedConVar<bool>(
			kBoolChannel,
			mConVarName,
			mWarned
		);
	}

	void UiConVarBoolCheckboxBehaviorComponent::RunChangedCommands() const {
		ExecuteCommands(kBoolChannel, mOnChangedCommands);
	}

	void UiConVarBoolCheckboxBehaviorComponent::WarnOnce(
		const std::string& message
	) const {
		if (mWarned) {
			return;
		}
		Warning(kBoolChannel, "{}", message);
		mWarned = true;
	}
}
