#pragma once
#include <runtime/core/math/Vec4.h>

namespace Unnamed {
	class ConsoleSystem;

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
		void ShowMenuBar() const;
		
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

		/// @brief 指定されたファイルパスと行番号で外部エディタを開きます
		/// @param file ファイルパス
		/// @param line 行番号
		/// @param column 列番号
		void OpenSourceFile(const std::string& file, int line, int column) const;


#ifdef _DEBUG
		static int InputTextCallback(ImGuiInputTextCallbackData* data);
#endif

		size_t FilteredToActualIndex(int filteredIndex);
		
		// --- Show() 分割 ---
		/// @brief ログテーブル（ヘッダ+行）を描画します
		void DrawLogTable(const ImVec2& childSize);
		/// @brief ログ行を描画します（フィルタ/選択/右クリックも含む）
		void DrawLogRows(int& visibleIndex, bool& requestOpenContextMenu);
		/// @brief 1行分の描画と入力処理を行います。描画した場合は true
		bool DrawLogRow(size_t actualIndex, int visibleIndex, bool& requestOpenContextMenu);
		/// @brief 選択中ログをクリップボードへコピーします（1要素=1行）
		void CopySelectedToClipboard() const;
		/// @brief Ctrl+C のコピー処理
		void HandleCopyShortcut() const;
		/// @brief 右クリックで開くコンテキストメニューを要求します
		static void OpenConsoleContextMenuPopup();

	private:
		ConsoleSystem* mConsoleSystem; // コンソールシステムへのポインタ

		bool mShowConsole = true;

		bool mWishScrollToBottom = false;

		char mInputBuffer[256] = "";
	};
}
