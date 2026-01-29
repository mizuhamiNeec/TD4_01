#pragma once
#include <unordered_map>

#include <engine/unnamed/subsystem/window/interface/IWindow.h>
#include <engine/unnamed/subsystem/window/interface/IWindowSystem.h>
#include <engine/unnamed/subsystem/window/Win32/Win32Window.h>

namespace Unnamed {
	struct IPlatformEvents;
}

/// @brief Win32ウィンドウシステムクラス
class Win32WindowSystem : public IWindowSystem {
public:
	~Win32WindowSystem() override;

	// IWindowSystem<-ISubsystem
	bool Init() override;
	void Update(float) override;
	void Shutdown() override;

	static void RegisterPlatformEvent(Unnamed::IPlatformEvents* events);

	[[nodiscard]] const std::string_view GetName() const override {
		return "Win32WindowSystem";
	}

	// Win32WindowSystem
	IWindow* CreateNewWindow(
		const IWindow::WindowCreateInfo& windowInfo
	) override;
	[[nodiscard]] const std::vector<std::unique_ptr<IWindow>>&
	GetWindows() const override;
	[[nodiscard]] bool AllClosed() const override;

	static void WishShutdown();

	[[nodiscard]] static bool IsInactiveWindow();

	static LRESULT CALLBACK WndProc(
		HWND   hWnd, UINT     msg,
		WPARAM wParam, LPARAM lParam
	);

private:
	void RegisterClassOnce();

	HINSTANCE   mHInstance  = {};
	std::string mClassName  = "UnnamedWindowClass";
	bool        mRegistered = false;

	static std::vector<std::unique_ptr<IWindow>>  mWindows;
	static std::unordered_map<HWND, Win32Window*> mHWndMap;

	static Unnamed::IPlatformEvents* mPlatformEvents;
};
