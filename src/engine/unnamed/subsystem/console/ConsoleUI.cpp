#ifdef _DEBUG
#include <imgui.h>
#include <imgui_internal.h>
#include <pch.h>
#include <string>

#include <core/math/Vec4.h>

#include <engine/ImGui/Icons.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/unnamed/subsystem/console/ConsoleUI.h>
#include <engine/unnamed/subsystem/console/ConVarHelper.h>
#include <engine/unnamed/subsystem/console/concommand/UnnamedConCommand.h>

#include "engine/Properties.h"

namespace Unnamed {
	static constexpr uint32_t kHistoryBufferSize = 64;
	static constexpr auto kConsoleUIContextPopupId = "##ConsoleUIContextMenu";

	namespace {
		/// @brief コンソールUIのデータ構造体
		struct ConsoleUIData {
			RingBuffer<std::string, kHistoryBufferSize> history = {};

			// 選択
			std::vector<bool> selected;
			int               lastSelectedFilteredIndex = -1;

			// フィルタ
			std::string currentFilterChannel;

			// 履歴ナビゲーション
			std::string scratch      = {}; // 一時保存用
			int         historyIndex = -1;
			bool        browsing     = false;

			// クリッピング用: フィルタ後の表示行 -> 実ログ(actualIndex) の対応
			std::vector<size_t> filteredToActual = {};
		} gConsoleUIData;
	}

	static constexpr auto kSubmitButtonText = " Submit ";

	static constexpr ImGuiWindowFlags kWindowFlags =
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_MenuBar;

	static constexpr ImGuiInputTextFlags kInputTextFlags =
		ImGuiInputTextFlags_EnterReturnsTrue |
		ImGuiInputTextFlags_CallbackCompletion |
		ImGuiInputTextFlags_CallbackHistory |
		ImGuiInputTextFlags_CallbackEdit |
		ImGuiInputTextFlags_CallbackResize;

	/// @brief コンストラクタ
	ConsoleUI::ConsoleUI(
		ConsoleSystem* consoleSystem
	) : mConsoleSystem(consoleSystem) {
		// ConVarヘルパーを初期化
		mConVarHelper = std::make_unique<ConVarHelper>(consoleSystem);
	}

	ConsoleUI::~ConsoleUI() {};

	void ConsoleUI::Init() {
		static UnnamedConCommand toggleconsole(
			"toggleconsole",
			[&](const std::vector<std::string>&) {
				mShowConsole = !mShowConsole;
				return true;
			},
			"Show/hide the console."
		);
	}

	/// @brief コンソールUIを表示します。
	/// @details ImGuiのコンテキスト内で呼び出し
	void ConsoleUI::Show() {
		// ConVarヘルパーは独立して表示
		if (mConVarHelper) {
			mConVarHelper->Show(mShowConVarHelper);
		}

		if (!mShowConsole) {
			return;
		}

		const bool windowOpen = ImGui::Begin(
			"Console##ConsoleUI",
			&mShowConsole,
			kWindowFlags
		);

		if (windowOpen) {
			ShowMenuBar();

			// 入力テキストの高さと子ウィンドウのサイズを計算
			const float inputTextHeight = ImGui::GetFrameHeightWithSpacing();
			const auto  childSize       = ImVec2(
				0, -inputTextHeight - ImGuiStyle().ItemSpacing.y
			);

			DrawLogTable(childSize);

			DrawInputTextAndSubmitButton();

			if (mShowAbout) {
				ShowAbout();
			}
		}

		ImGui::End();
	}

	void ConsoleUI::OnConsoleUpdate() {
		mWishScrollToBottom = true;
	}

	void ConsoleUI::ShowMenuBar() {
		if (ImGui::BeginMenuBar()) {
			ImGui::PushStyleVar(
				ImGuiStyleVar_WindowPadding,
				ImVec2(kPopupPadding, kPopupPadding)
			);

			if (ImGui::BeginMenu("File")) {
				if (ImGuiWidgets::MenuItemWithIcon("Clear", kIconReset)) {
					// コンソール出力をクリア
					mConsoleSystem->ExecuteCommand("clear");
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("コンソールの履歴をクリアします。");
					ImGui::EndTooltip();
				}

				ImGui::Separator();

				if (ImGuiWidgets::MenuItemWithIcon("Close", kIconClose)) {
					// コンソールを閉じる
					mShowConsole = false;
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("コンソールを閉じます");
					ImGui::EndTooltip();
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Tools")) {
				if (ImGuiWidgets::MenuItemWithIcon(
					"ConVar Helper", kIconSettings, nullptr, mShowConVarHelper
				)) {
					// ConVarヘルパーを表示/非表示
					mShowConVarHelper = !mShowConVarHelper;
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("ConVarヘルパーウィンドウを表示/非表示します。");
					ImGui::EndTooltip();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help")) {
				if (ImGuiWidgets::MenuItemWithIcon("Help", kIconHelp)) {
					mConsoleSystem->ExecuteCommand("help");
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("ヘルプを表示します。");
					ImGui::EndTooltip();
				}
				if (ImGuiWidgets::MenuItemWithIcon(
					"About", kIconInfo, nullptr, mShowAbout
				)) {
					mShowAbout = true;
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("コンソールUIについて。");
					ImGui::EndTooltip();
				}
				ImGui::EndMenu();
			}

			ImGui::PopStyleVar();

			ImGui::EndMenuBar();
		}
	}

	void ConsoleUI::DrawInputTextAndSubmitButton() {
		const ImGuiStyle& style   = ImGui::GetStyle();
		const float       availW  = ImGui::GetContentRegionAvail().x;
		const float       submitW =
			ImGui::CalcTextSize(kSubmitButtonText).x +
			style.FramePadding.x *
			2.0f;
		const float spacingW = style.ItemSpacing.x;

		const float  inputW = std::max(availW - submitW - spacingW, 0.0f);
		const ImVec2 inputSize{inputW, 0.0f}; // 高さは任せる

		// 下端に寄せる
		const float lineH   = ImGui::GetFrameHeightWithSpacing();
		const float remainH = ImGui::GetContentRegionAvail().y;
		if (remainH > lineH) {
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (remainH - lineH));
		}

		ImGui::SetNextItemWidth(inputW);
		if (ImGui::InputTextEx(
			"##Input",
			nullptr,
			mInputBuffer,
			IM_ARRAYSIZE(mInputBuffer),
			inputSize,
			kInputTextFlags,
			InputTextCallback,
			this
		)) {
			Submit();
		}

		ImGui::SameLine(0.0f, spacingW);

		if (ImGui::Button(kSubmitButtonText)) {
			Submit();
		}
	}

	void ConsoleUI::ShowAbout() {
		const std::string text =
			"About " + std::string(ENGINE_NAME) + " Console";
		ImGui::OpenPopup(text.c_str());

		const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(
			center, 1, ImVec2(0.5f, 0.5f)
		);

		constexpr ImVec2 windowSize = {280.0f, 230.0f};
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);

		constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoSavedSettings;

		const bool bOpen = ImGui::BeginPopupModal(
			text.c_str(), &mShowAbout, flags
		);

		if (bOpen) {
			ImDrawList*     dl        = ImGui::GetWindowDrawList();
			ImFont*         font      = ImGui::GetFont();
			constexpr float iconSize  = 70.0f;
			constexpr float titleSize = 24.0f;
			constexpr float margin    = 8.0f;

			// アイコンを描画
			dl->AddText(
				font, iconSize, ImGui::GetCursorScreenPos(),
				ImGui::GetColorU32(ImGuiCol_Text),
				StrUtil::ConvertToUtf8(kIconTerminal).c_str()
			);

			// アイコン分だけ右にカーソルを移動
			ImGui::SetCursorPosX(iconSize + margin);

			// タイトルを描画
			dl->AddText(
				font, titleSize, ImGui::GetCursorScreenPos(),
				ImGui::GetColorU32(ImGuiCol_Text),
				"Unnamed Console"
			);

			ImGui::Dummy({iconSize + margin, titleSize});

			// ただ2回スペース空けてカーソルを移動する関数
			auto doublespacing = [&] {
				for (int i = 0; i < 2; ++i) {
					ImGui::Spacing();
				}
				ImGui::SetCursorPosX(iconSize + margin);
			};

			doublespacing();
			ImGui::Text("Version: %s", std::string(ENGINE_VERSION).c_str());
			doublespacing();
			ImGui::Text(
				"Build: %s %s", std::string(__DATE__).c_str(),
				std::string(__TIME__).c_str()
			);
			doublespacing();
			ImGui::Text("ImGui Version: %s", ImGui::GetVersion());
			doublespacing();
			// ボタンサイズとウィンドウサイズを取得
			constexpr auto buttonSize     = ImVec2(74.0f, 24.0f);
			const ImVec2   clientSize     = ImGui::GetWindowSize(); // サイズ
			const ImVec2   cursorStartPos = ImGui::GetCursorPos();  // 現在のカーソル位置
			// ボタンを中央に配置
			const float buttonPosX =
				(clientSize.x - buttonSize.x) * 0.5f; // 中央揃えのX座標
			const float buttonPosY =
				cursorStartPos.y +
				ImGui::GetContentRegionAvail().y -
				buttonSize.y; // 下端からの位置調整
			ImGui::SetCursorPos(ImVec2(buttonPosX, buttonPosY));

			if (ImGui::Button("Close", buttonSize)) {
				mShowAbout = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void ConsoleUI::ShowContextMenu() const {
		ImGui::PushStyleVar(
			ImGuiStyleVar_WindowPadding,
			ImVec2(kPopupPadding, kPopupPadding)
		);

		if (ImGui::BeginPopup(kConsoleUIContextPopupId)) {
			// 選択数をカウント
			int    selectedCount = 0;
			size_t selectedIndex = 0;
			for (size_t i = 0; i < gConsoleUIData.selected.size(); ++i) {
				if (gConsoleUIData.selected[i]) {
					selectedCount++;
					selectedIndex = i;
				}
			}

			if (selectedCount == 1) {
				// 単一選択時のみ、ソース位置と「Open Source File」を表示
				const auto& buffer = mConsoleSystem->GetLogBuffer()[
					selectedIndex];
				ImGui::TextDisabled(
					"File: %s", buffer.location.file_name()
				);
				ImGui::TextDisabled("Line: %d", buffer.location.line());

				if (ImGuiWidgets::MenuItemWithIcon(
					"Open Source File",
					kIconBox
				)) {
					OpenSourceFile(
						buffer.location.file_name(),
						buffer.location.line(),
						buffer.location.column()
					);
				}
				ImGui::Separator();
			}

			// 複数選択時は「Copy Selected」のみ表示
			if (
				ImGuiWidgets::MenuItemWithIcon(
					"Copy Selected",
					kIconCopy
				)
			) {
				CopySelectedToClipboard();
			}

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();
	}

	void ConsoleUI::Submit() {
		// 送信してもフォーカスは維持
		ImGui::SetKeyboardFocusHere(-1);

		const auto cmd = std::string(mInputBuffer);

		// 空は履歴に入れない（実行は ConsoleSystem 側に任せる）
		if (!cmd.empty()) {
			gConsoleUIData.history.Push(cmd);
		}

		// 履歴ナビゲーションの状態をリセット
		gConsoleUIData.historyIndex =
			static_cast<int>(gConsoleUIData.history.Size());
		gConsoleUIData.scratch.clear();
		gConsoleUIData.browsing = false;

		// コンソールシステムに送る
		mConsoleSystem->ExecuteCommand(cmd);
		mInputBuffer[0] = '\0';

		// コマンドが送信時に一番下までスクロールする
		mWishScrollToBottom = true;
	}

	void ConsoleUI::CheckScroll() {
		// スクロールが一番下にある場合、自動スクロールを行う
		if (
			mWishScrollToBottom &&
			ImGui::GetScrollY() >= ImGui::GetScrollMaxY()
		) {
			ImGui::SetScrollHereY(1.0f);
		}

		// スクロール位置をチェックし、状態を更新
		if (ImGui::GetScrollY() < ImGui::GetScrollMaxY()) {
			mWishScrollToBottom = false;
		} else {
			mWishScrollToBottom = true;
		}
	}

	void ConsoleUI::PushTextColor(const ConsoleLogText& buffer) {
		switch (buffer.level) {
			case LogLevel::None:
			case LogLevel::Info: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColor)
				);
				break;
			case LogLevel::Dev: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorDev)
				);
				break;
			case LogLevel::Warning: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorWarn)
				);
				break;
			case LogLevel::Error: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorError)
				);
				break;
			case LogLevel::Fatal: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorFatal)
				);
				break;
			case LogLevel::Execute: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorExec)
				);
				break;
			case LogLevel::Waiting: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorWait)
				);
				break;
			case LogLevel::Success: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorSuccess)
				);
				break;
		}
	}

	void ConsoleUI::OpenSourceFile(
		const std::string& file, int line, int column
	) const {
		const std::string command = std::format(
			"--line {} --column {} {}",
			line,
			column,
			file
		);
		ShellExecuteW(
			nullptr,
			L"open",
			L"Rider.cmd",
			StrUtil::ToWString(command).c_str(),
			nullptr,
			SW_HIDE
		);
	}

	/// @brief インプットテキストからのコールバック
	int ConsoleUI::InputTextCallback(ImGuiInputTextCallbackData* data) {
		auto& c = gConsoleUIData;
		switch (data->EventFlag) {
			case ImGuiInputTextFlags_CallbackCompletion: {
				Msg("callback", "completion");
			}
			break;

			case ImGuiInputTextFlags_CallbackHistory: {
				const int historySize = static_cast<int>(c.history.Size());
				if (historySize <= 0) {
					break;
				}

				// ↑↓を押したら入力中だったテキストを scratch に保存
				if (!c.browsing) {
					c.scratch.assign(data->Buf, data->BufTextLen);
					c.browsing     = true;
					c.historyIndex = historySize; // 末尾（scratch位置）から開始
				}

				const int prevHistoryIndex = c.historyIndex;
				if (data->EventKey == ImGuiKey_UpArrow) {
					if (c.historyIndex > 0) {
						c.historyIndex--;
					}
				} else if (data->EventKey == ImGuiKey_DownArrow) {
					if (c.historyIndex < historySize) {
						c.historyIndex++;
					} else {
						c.historyIndex = historySize;
					}
				}

				if (prevHistoryIndex != c.historyIndex) {
					data->DeleteChars(0, data->BufTextLen);
					if (c.historyIndex < historySize) {
						data->InsertChars(
							0, c.history[static_cast<size_t>(c.historyIndex)].
							c_str()
						);
					} else {
						// 末尾に戻ったら 入力中テキストを復元
						data->InsertChars(0, c.scratch.c_str());
					}
				}
				break;
			}

			case ImGuiInputTextFlags_CallbackEdit:
				// 編集が入ったら「履歴閲覧中」フラグを解除（次の↑↓で scratch を取り直す）
				c.browsing = false;
				break;

			case ImGuiInputTextFlags_CallbackResize: {}
			break;
			default: ;
		}
		return 0;
	}

	size_t ConsoleUI::FilteredToActualIndex(const int filteredIndex) {
		if (filteredIndex < 0) {
			return SIZE_MAX;
		}
		const auto idx = static_cast<size_t>(filteredIndex);
		if (idx >= gConsoleUIData.filteredToActual.size()) {
			return SIZE_MAX;
		}
		return gConsoleUIData.filteredToActual[idx];
	}

	void ConsoleUI::DrawLogTable(const ImVec2& childSize) {
		if (!ImGui::BeginTable(
			"Show##ConsoleUI", 2,
			ImGuiTableFlags_ScrollX |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit |
			ImGuiTableFlags_BordersV |
			ImGuiTableFlags_NoHostExtendX,
			childSize
		)) {
			return;
		}

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Log", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		int  visibleIndex           = 0;
		bool requestOpenContextMenu = false;

		// 選択バッファはログ数に合わせる
		gConsoleUIData.selected.resize(
			mConsoleSystem->GetLogBuffer().Size(), false
		);

		// フィルタ後の表示行 から actualIndex マップを作成
		gConsoleUIData.filteredToActual.clear();
		gConsoleUIData.filteredToActual.reserve(
			mConsoleSystem->GetLogBuffer().Size()
		);
		for (size_t i = 0; i < mConsoleSystem->GetLogBuffer().Size(); ++i) {
			const auto& buffer = mConsoleSystem->GetLogBuffer()[i];
			if (
				gConsoleUIData.currentFilterChannel == kChannelNone ||
				buffer.channel == gConsoleUIData.currentFilterChannel
			) {
				gConsoleUIData.filteredToActual.push_back(i);
			}
		}

		DrawLogRows(visibleIndex, requestOpenContextMenu);

		if (requestOpenContextMenu) {
			OpenConsoleContextMenuPopup();
		}
		ShowContextMenu();
		HandleCopyShortcut();
		CheckScroll();

		ImGui::EndTable();
	}

	void ConsoleUI::DrawLogRows(
		int& visibleIndex, bool& requestOpenContextMenu
	) {
		// ImGuiListClipper でスクロール範囲外の行の描画/入力処理を省略
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(gConsoleUIData.filteredToActual.size()));
		while (clipper.Step()) {
			for (
				int row = clipper.DisplayStart;
				row < clipper.DisplayEnd;
				++row
			) {
				const size_t actualIndex = gConsoleUIData.filteredToActual[
					static_cast<size_t>(row)
				];
				if (DrawLogRow(
					actualIndex,
					row,
					requestOpenContextMenu
				)) {
					visibleIndex++;
				}
			}
		}
	}

	bool ConsoleUI::DrawLogRow(
		const size_t actualIndex,
		const int    visibleIndex,
		bool&        requestOpenContextMenu
	) {
		const auto& buffer = mConsoleSystem->GetLogBuffer()[actualIndex];

		std::string display = buffer.message;
		display             += "##" + std::to_string(actualIndex);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);

		PushTextColor(buffer);
		ImGui::PushID(static_cast<int>(actualIndex));

		const bool isSelected = gConsoleUIData.selected[actualIndex];
		ImGui::Selectable(
			display.c_str(),
			isSelected,
			ImGuiSelectableFlags_SpanAllColumns |
			ImGuiSelectableFlags_AllowOverlap
		);

		// 左クリック選択
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			if (ImGui::GetIO().KeyCtrl) {
				gConsoleUIData.selected[actualIndex] =
					!gConsoleUIData.selected[actualIndex];
				gConsoleUIData.lastSelectedFilteredIndex = visibleIndex;
			} else if (
				ImGui::GetIO().KeyShift &&
				gConsoleUIData.lastSelectedFilteredIndex != -1
			) {
				if (!ImGui::GetIO().KeyCtrl) {
					std::ranges::fill(gConsoleUIData.selected, false);
				}

				const int start = std::min(
					gConsoleUIData.lastSelectedFilteredIndex, visibleIndex
				);
				const int end = std::max(
					gConsoleUIData.lastSelectedFilteredIndex, visibleIndex
				);
				for (int j = start; j <= end; ++j) {
					const size_t idx = FilteredToActualIndex(j);
					if (idx != SIZE_MAX) {
						gConsoleUIData.selected[idx] = true;
					}
				}
			} else {
				std::ranges::fill(gConsoleUIData.selected, false);
				gConsoleUIData.selected[actualIndex]     = true;
				gConsoleUIData.lastSelectedFilteredIndex = visibleIndex;
			}
		}

		// 右クリック: 選択済みなら選択維持、未選択なら単一選択へ
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			if (!gConsoleUIData.selected[actualIndex]) {
				std::ranges::fill(gConsoleUIData.selected, false);
				gConsoleUIData.selected[actualIndex]     = true;
				gConsoleUIData.lastSelectedFilteredIndex = visibleIndex;
			}
			requestOpenContextMenu = true;
		}

		ImGui::PopID();
		ImGui::PopStyleColor();

		// チャンネル列
		ImGui::TableSetColumnIndex(1);
		if (buffer.channel != kChannelNone) {
			if (ImGui::TextLink(
				(buffer.channel + "##" + std::to_string(actualIndex)).c_str()
			)) {
				if (gConsoleUIData.currentFilterChannel == buffer.channel) {
					gConsoleUIData.currentFilterChannel = kChannelNone;
				} else {
					gConsoleUIData.currentFilterChannel = buffer.channel;
				}
			}
		}

		return true;
	}

	void ConsoleUI::CopySelectedToClipboard() const {
		std::string      copiedText;
		constexpr size_t kMaxCopyLength = static_cast<size_t>(1024) * 1024;
		// 1MB

		for (size_t i = 0; i < mConsoleSystem->GetLogBuffer().Size(); ++i) {
			if (!gConsoleUIData.selected[i]) {
				continue;
			}

			const auto&  buffer       = mConsoleSystem->GetLogBuffer()[i];
			const size_t requiredSize =
				copiedText.size() + buffer.message.size() + 1;

			if (requiredSize > kMaxCopyLength) {
				if (!copiedText.empty() && copiedText.back() != '\n') {
					copiedText += '\n';
				}
				copiedText += "... (copy truncated) ...\n";
				break;
			}

			copiedText += buffer.message;
			copiedText += '\n';
		}

		ImGui::SetClipboardText(copiedText.c_str());
	}

	void ConsoleUI::HandleCopyShortcut() const {
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
			CopySelectedToClipboard();
		}
	}

	void ConsoleUI::OpenConsoleContextMenuPopup() {
		ImGui::OpenPopup(kConsoleUIContextPopupId);
	}
}
#endif
