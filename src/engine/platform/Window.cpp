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
				DestroyWindow(hwnd);
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

	void Window::MarkCloseRequested() { mShouldClose = true; }
}
