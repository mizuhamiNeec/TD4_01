#pragma once

#ifdef _WIN64
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <engine/unnamed/subsystem/window/interface/IWindow.h>

/// @brief Win32ウィンドウクラス
class Win32Window : public IWindow {
public:
	// IWindow
	[[nodiscard]] bool ShouldClose() const override;
	[[nodiscard]] WindowCreateInfo GetInfo() const override;
	[[nodiscard]] void* GetNativeHandle() const override;
	void RequestClose() override;
	void SetTitle(std::string_view title) override;
	void SetInactive(bool flag) override;
	void OnResize(uint32_t width, uint32_t height) override;

	// Win32Window


	explicit Win32Window(WindowCreateInfo windowInfo)
		: mWindowInfo(std::move(windowInfo)) {}

	~Win32Window() override;
	bool Create(
		HINSTANCE               hInstance,
		const std::string_view& className,
		HWND                    hParent = nullptr
	);

	void SetImmersiveDarkMode(bool darkMode) const;

private:
	WindowCreateInfo mWindowInfo;
	HWND             mHWnd        = nullptr;
	bool             mShouldClose = false;
};
#endif
