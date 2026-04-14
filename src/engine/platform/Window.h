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
		int32_t width        = 0;
		int32_t height       = 0;
		bool    isLiveResize = false;
	};

	class Window final {
	public:
		Window(WindowId id, WindowDesc desc, HWND hwnd);

		/// @brief ウィンドウのIDを取得します。
		[[nodiscard]] WindowId GetId() const;

		/// @brief ウィンドウのHWNDを取得します。
		[[nodiscard]] HWND GetHwnd() const;

		/// @brief ウィンドウの説明を取得します。
		[[nodiscard]] WindowDesc GetDesc() const;

		/// @brief ウィンドウが閉じるべきかどうかを返します。WindowManagerがこれを見て実際の破棄を行います。
		[[nodiscard]] bool ShouldClose() const;

		/// @brief ウィンドウが最小化されているかどうかを返します。
		[[nodiscard]] bool IsMinimized() const;

		/// @brief 保留中のリサイズイベントを消費します。リサイズイベントがない場合はstd::nulloptを返します。
		std::optional<WindowResizeEvent> ConsumeResizeEvent();

		/// @brief ウィンドウメッセージを処理します。WindowManagerから呼び出されます。
		LRESULT HandleMessage(
			HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
		);

		/// @brief フルスクリーンとウィンドウモードを切り替えます。
		void ToggleFullscreen() const;

	private:
		/// @brief ウィンドウを閉じるようにマークします。実際の破棄はWindowManagerが行います。
		void MarkCloseRequested();

		HWND              mHwnd          = nullptr;
		WindowDesc        mDesc          = {};
		WindowResizeEvent mPendingResize = {};
		WindowId          mId            = {};

		bool mShouldClose      = false;
		bool mMinimized        = false;
		bool mHasPendingResize = false;
		bool mInLiveResize     = false;
	};
}
