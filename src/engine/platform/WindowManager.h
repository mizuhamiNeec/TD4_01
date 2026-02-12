#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "IPlatformEvents.h"
#include "Window.h"

#include "engine/EngineConfig.h"
#include "engine/unnamed/subsystem/input/UInputSystem.h"

namespace Unnamed {
	class WindowManager final {
	public:
		WindowManager();
		~WindowManager();

		bool Init(const EngineConfig::Window& mainWindowConfig);
		void Shutdown();

		/// @brief メッセージポンプを実行する
		void ProcessMessage();

		/// @brief 終了すべきかどうかを返す
		[[nodiscard]] bool ShouldQuit() const;

		/// @brief メインウィンドウのIDを取得する
		[[nodiscard]] WindowId GetMainWindowId() const;

		/// @brief 新しいウィンドウを作成する
		/// @param desc ウィンドウの説明
		/// @return 作成したウィンドウのID。失敗した場合はstd::nulloptを返す
		std::optional<WindowId> CreateNewWindow(const WindowDesc& desc);

		/// @brief ウィンドウを破棄する
		/// @param id 破棄するウィンドウのID
		void DestroyWindow(WindowId id);

		/// @brief IDからウィンドウを探す
		[[nodiscard]]
		Window* FindWindowById(WindowId id);

		/// @brief IDからウィンドウを探す (const版)
		[[nodiscard]] const Window* FindWindowById(WindowId id) const;

		/// @brief すべてのウィンドウIDを取得する
		[[nodiscard]] std::vector<WindowId> GetAllWindowIds() const;

		void RegisterPlatformEvents(IPlatformEvents* events);

	private:
		bool                    EnsureWindowClassRegistered();
		std::optional<HWND>     CreateNativeWindow(const WindowDesc& desc);
		static LRESULT CALLBACK StaticWndProc(
			HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
		);

		/// @brief ウィンドウのダークモード設定を変更する
		/// @param hwnd ウィンドウハンドル
		/// @param isSystemDarkTheme ダークテーマを使用するかどうか
		static void SetUseImmersiveDarkMode(HWND hwnd, bool isSystemDarkTheme);

		WindowId mMainWindowId = {};
		uint32_t mNextWindowId = 1;
		std::unordered_map<uint32_t, std::unique_ptr<Window>> mWindows;

		static IPlatformEvents* mPlatformEvents;

		bool mInitialized           = false;
		bool mWindowClassRegistered = false;
	};
}
