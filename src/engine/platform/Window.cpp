#include "Window.h"

namespace Unnamed {
	Window::Window(const WindowId id, WindowDesc desc, const HWND hwnd) :
		mHwnd(hwnd),
		mDesc(std::move(desc)),
		mId(id) {}

	WindowId Window::GetId() const { return mId; }

	HWND Window::GetHwnd() const { return mHwnd; }

	WindowDesc Window::GetDesc() const { return mDesc; }

	bool Window::ShouldClose() const { return mShouldClose; }

	bool Window::IsMinimized() const { return mMinimized; }

	std::optional<WindowResizeEvent> Window::ConsumeResizeEvent() {
		if (!mHasPendingResize) return std::nullopt;

		mDesc.width  = mPendingResize.width;
		mDesc.height = mPendingResize.height;

		mHasPendingResize = false;
		return mPendingResize;
	}

	LRESULT Window::HandleMessage(
		const HWND   hwnd, const UINT     msg,
		const WPARAM wParam, const LPARAM lParam
	) {
		switch (msg) {
			case WM_CLOSE: {
				MarkCloseRequested();
				return 0;
			}

			case WM_DESTROY: {
				MarkCloseRequested();
				return 0;
			}

			case WM_SIZE: {
				const int32_t width  = LOWORD(lParam);
				const int32_t height = HIWORD(lParam);

				if (wParam == SIZE_MINIMIZED) { mMinimized = true; } else {
					mMinimized            = false;
					mHasPendingResize     = true;
					mPendingResize.width  = width;
					mPendingResize.height = height;
				}
				return 0;
			}

			default: break;
		}

		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	void Window::ToggleFullscreen() {
		// フルスクリーン切り替え
		const HWND hwnd = GetHwnd();
		if (
			const DWORD style = GetWindowLong(hwnd, GWL_STYLE);
			style & WS_OVERLAPPEDWINDOW
		) {
			MONITORINFO mi = {sizeof(mi)};
			if (GetMonitorInfo(
				MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi
			)) {
				SetWindowLong(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
				SetWindowPos(
					hwnd,
					HWND_TOP,
					mi.rcMonitor.left,
					mi.rcMonitor.top,
					mi.rcMonitor.right - mi.rcMonitor.left,
					mi.rcMonitor.bottom - mi.rcMonitor.top,
					SWP_NOOWNERZORDER | SWP_FRAMECHANGED
				);
			}
		} else {
			SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
			SetWindowPos(
				hwnd,
				HWND_NOTOPMOST,
				100,
				100,
				mDesc.width,
				mDesc.height,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED
			);
		}
	}

	void Window::MarkCloseRequested() { mShouldClose = true; }
}
