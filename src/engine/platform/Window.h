#pragma once
#include <cstdint>
#include <optional>
#include <string>

#include <Windows.h>

#include "engine/EngineConfig.h"

namespace Unnamed {
	struct WindowId {
		uint32_t value = 0;

		friend bool operator==(const WindowId a, const WindowId b) {
			return a.value == b.value;
		}

		friend bool operator!=(const WindowId a, const WindowId b) {
			return !(a == b);
		}
	};

	struct WindowDesc {
		std::string title     = "Unnamed Window";
		int32_t     width     = 1280;
		int32_t     height    = 720;
		WINDOW_MODE mode      = WINDOW_MODE::WINDOWED;
		bool        resizable = true;
		bool        visible   = true;
	};

	struct WindowResizeEvent {
		int32_t width  = 0;
		int32_t height = 0;
	};

	class Window final {
	public:
		Window(WindowId id, WindowDesc desc, HWND hwnd);

		[[nodiscard]] WindowId GetId() const;
		[[nodiscard]] HWND     GetHwnd() const;

		[[nodiscard]] WindowDesc GetDesc() const;

		[[nodiscard]] bool ShouldClose() const;
		[[nodiscard]] bool IsMinimized() const;

		std::optional<WindowResizeEvent> ConsumeResizeEvent();

		LRESULT HandleMessage(
			HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
		);
		
		void ToggleFullscreen();

	private:
		void MarkCloseRequested();

	private:
		HWND              mHwnd          = nullptr;
		WindowDesc        mDesc          = {};
		WindowResizeEvent mPendingResize = {};
		WindowId          mId            = {};

		bool mShouldClose      = false;
		bool mMinimized        = false;
		bool mHasPendingResize = false;
	};
}
