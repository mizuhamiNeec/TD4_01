#pragma once
#include <runtime/core/math/Vec4.h>

namespace Unnamed {
	class ConsoleSystem;

	constexpr Vec4 kConTextColor        = {0.71f, 0.71f, 0.72f, 1.0f};
	constexpr Vec4 kConTextColorDev     = {0.25f, 0.71f, 0.25f, 1.0f};
	constexpr Vec4 kConTextColorWarn    = {0.93f, 0.79f, 0.09f, 1.0f};
	constexpr Vec4 kConTextColorError   = {0.71f, 0.25f, 0.25f, 1.0f};
	constexpr Vec4 kConTextColorFatal   = {0.71f, 0.0f, 0.0f, 1.0f};
	constexpr Vec4 kConTextColorExec    = {0.8f, 1.0f, 1.0f, 1.0f};
	constexpr Vec4 kConTextColorWait    = {0.93f, 0.79f, 0.09f, 1.0f};
	constexpr Vec4 kConTextColorSuccess = {0.48f, 0.76f, 0.26f, 1.0f};

	/// @brief コンソールUIクラス
	class ConsoleUI {
	public:
		explicit ConsoleUI(ConsoleSystem* consoleSystem);

		/// @brief 初期化
		void Init();

		/// @brief コンソールUIを表示します
		void Show();

		/// @brief コンソールが更新された際のイベント
		void OnConsoleUpdate();

	private:
		/// @brief メニューバーを表示します
		void ShowMenuBar();
		
		/// @brief 入力欄と送信ボタンを描画します
		void DrawInputTextAndSubmitButton();

		/// @brief コンソールのコンテキストメニューを表示します
		void ShowContextMenu() const;

		/// @brief 入力されたコマンドを送信します
		void Submit();

		/// @brief スクロール状態をチェックし、自動スクロールを行います
		void CheckScroll();

		/// @brief コンソールログのテキストの色を設定します。
		/// @param buffer コンソールログのテキスト情報
		static void PushTextColor(const struct ConsoleLogText& buffer);


#ifdef _DEBUG
		static int InputTextCallback(ImGuiInputTextCallbackData* data);
#endif

		size_t FilteredToActualIndex(int filteredIndex);

	private:
		ConsoleSystem* mConsoleSystem; // コンソールシステムへのポインタ

		bool mShowConsole = true;

		bool mWishScrollToBottom = false;

		char mInputBuffer[256] = "";
	};
}
