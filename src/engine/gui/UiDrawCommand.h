#pragma once
#include <string>

#include "Rect.h"

namespace Unnamed::Gui {
	struct Color {
		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;
		float a = 1.0f;
	};

	enum class UiDrawCommandType {
		RECT,
		TEXT,
	};

	struct UiDrawCommandRect {
		Rect  rect;
		Color fillColor;
		float cornerRadius    = 0.0f;
		float borderThickness = 0.0f;
		Color borderColor     = {.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f};
	};

	struct UiDrawCommandText {
		Vec2        position;
		std::string text;
		Color       color;
		float       fontSize = 16.0f;
	};

	struct UiDrawCommand {
		UiDrawCommandType type{UiDrawCommandType::RECT};
		UiDrawCommandRect rect;
		UiDrawCommandText text;
	};
}
