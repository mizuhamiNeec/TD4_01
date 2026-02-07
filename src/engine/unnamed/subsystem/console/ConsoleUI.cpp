#ifdef _DEBUG
#include <pch.h>
#include <string>

#include <engine/unnamed/subsystem/console/ConsoleUI.h>

#include <imgui.h>
#include <imgui_internal.h>

#include "concommand/UnnamedConCommand.h"

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"

namespace Unnamed {
	static constexpr uint32_t kHistoryBufferSize = 64;
	static constexpr auto kConsoleUIContextPopupId = "##ConsoleUIContextMenu";

	constexpr Vec4 kConTextColor        = {0.71f, 0.71f, 0.72f, 1.0f};
	constexpr Vec4 kConTextColorDev     = {0.25f, 0.71f, 0.25f, 1.0f};
	constexpr Vec4 kConTextColorWarn    = {0.93f, 0.79f, 0.09f, 1.0f};
	constexpr Vec4 kConTextColorError   = {0.71f, 0.25f, 0.25f, 1.0f};
	constexpr Vec4 kConTextColorFatal   = {0.71f, 0.0f, 0.0f, 1.0f};
	constexpr Vec4 kConTextColorExec    = {0.8f, 1.0f, 1.0f, 1.0f};
	constexpr Vec4 kConTextColorWait    = {0.93f, 0.79f, 0.09f, 1.0f};
	constexpr Vec4 kConTextColorSuccess = {0.48f, 0.76f, 0.26f, 1.0f};

	namespace {
		/// @brief コンソールUIのデータ構造体
		struct ConsoleUIData {
			RingBuffer<std::string, kHistoryBufferSize> history = {};

			// 選択
			std::vector<bool> selected;
			int               lastSelectedIndex = -1;

			// フィルタ
			std::string currentFilterChannel;

			// 履歴ナビゲーション
			std::string scratch      = {}; // 一時保存用
			int         historyIndex = -1;
			bool        browsing     = false;
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
	) : mConsoleSystem(consoleSystem) {}

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
		if (!mShowConsole) { return; }

		// フィルター可能なチャンネルを集める

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
		}

		ImGui::End();
	}

	void ConsoleUI::OnConsoleUpdate() { mWishScrollToBottom = true; }

	void ConsoleUI::ShowMenuBar() const {
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (
					ImGuiWidgets::MenuItemWithIcon(
						StrUtil::ConvertToUtf8(kIconReset).c_str(), "Clear Log"
					)
				) { mConsoleSystem->ExecuteCommand("clear"); }
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help")) {
				if (ImGui::MenuItem("Help", "F1")) {
					mConsoleSystem->ExecuteCommand("help");
				}
				if (
					ImGuiWidgets::MenuItemWithIcon(
						StrUtil::ConvertToUtf8(kIconInfo).c_str(), "About"
					)
				) {}
				ImGui::EndMenu();
			}
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
		)) { Submit(); }

		ImGui::SameLine(0.0f, spacingW);

		if (ImGui::Button(kSubmitButtonText)) { Submit(); }
	}

	void ConsoleUI::ShowContextMenu() const {
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
					StrUtil::ConvertToUtf8(kIconBox).c_str(),
					" Open Source File"
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
					StrUtil::ConvertToUtf8(kIconCopy).c_str(),
					" Copy Selected"
				)
			) { CopySelectedToClipboard(); }

			ImGui::EndPopup();
		}
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
		)) { return; }

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Log", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		int  visibleIndex           = 0;
		bool requestOpenContextMenu = false;

		gConsoleUIData.selected.resize(
			mConsoleSystem->GetLogBuffer().Size(), false
		);

		DrawLogRows(visibleIndex, requestOpenContextMenu);

		if (requestOpenContextMenu) { OpenConsoleContextMenuPopup(); }
		ShowContextMenu();
		HandleCopyShortcut();
		CheckScroll();

		ImGui::EndTable();
	}

	void ConsoleUI::DrawLogRows(
		int& visibleIndex, bool& requestOpenContextMenu
	) {
		for (size_t i = 0; i < mConsoleSystem->GetLogBuffer().Size(); ++i) {
			if (DrawLogRow(i, visibleIndex, requestOpenContextMenu)) {
				visibleIndex++;
			}
		}
	}

	bool ConsoleUI::DrawLogRow(
		size_t actualIndex,
		int    visibleIndex,
		bool&  requestOpenContextMenu
	) {
		const auto& buffer = mConsoleSystem->GetLogBuffer()[actualIndex];

		// フィルターに一致しない場合はスキップ
		if (gConsoleUIData.currentFilterChannel != kChannelNone &&
		    buffer.channel != gConsoleUIData.currentFilterChannel) {
			return false;
		}

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
			ImGuiSelectableFlags_AllowItemOverlap
		);

		// 左クリック選択
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			if (ImGui::GetIO().KeyCtrl) {
				gConsoleUIData.selected[actualIndex] = !gConsoleUIData.selected[
					actualIndex];
				gConsoleUIData.lastSelectedIndex = static_cast<int>(
					actualIndex);
			} else if (ImGui::GetIO().KeyShift && gConsoleUIData.
			           lastSelectedIndex != -1) {
				if (!ImGui::GetIO().KeyCtrl) {
					std::ranges::fill(gConsoleUIData.selected, false);
				}

				const int start = std::min(
					gConsoleUIData.lastSelectedIndex, visibleIndex
				);
				const int end = std::max(
					gConsoleUIData.lastSelectedIndex, visibleIndex
				);
				for (int j = start; j <= end; ++j) {
					const size_t idx = FilteredToActualIndex(j);
					if (idx != SIZE_MAX) {
						gConsoleUIData.selected[idx] = true;
					}
				}
			} else {
				std::ranges::fill(gConsoleUIData.selected, false);
				gConsoleUIData.selected[actualIndex] = true;
				gConsoleUIData.lastSelectedIndex     = static_cast<int>(
					actualIndex);
			}
		}

		// 右クリック: 選択済みなら選択維持、未選択なら単一選択へ
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			if (!gConsoleUIData.selected[actualIndex]) {
				std::ranges::fill(gConsoleUIData.selected, false);
				gConsoleUIData.selected[actualIndex] = true;
				gConsoleUIData.lastSelectedIndex     = static_cast<int>(
					actualIndex);
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
				} else { gConsoleUIData.currentFilterChannel = buffer.channel; }
			}
		}

		return true;
	}

	void ConsoleUI::OpenConsoleContextMenuPopup() {
		ImGui::OpenPopup(kConsoleUIContextPopupId);
	}

	void ConsoleUI::CopySelectedToClipboard() const {
		std::string      copiedText;
		constexpr size_t kMaxCopyLength = static_cast<size_t>(1024) * 1024;
		// 1MB

		for (size_t i = 0; i < mConsoleSystem->GetLogBuffer().Size(); ++i) {
			if (!gConsoleUIData.selected[i]) { continue; }

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

	void ConsoleUI::Submit() {
		// 送信してもフォーカスは維持
		ImGui::SetKeyboardFocusHere(-1);

		std::string cmd = std::string(mInputBuffer);

		// 空は履歴に入れない（実行は ConsoleSystem 側に任せる）
		if (!cmd.empty()) { gConsoleUIData.history.Push(cmd); }

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
		) { ImGui::SetScrollHereY(1.0f); }

		// スクロール位置をチェックし、状態を更新
		if (ImGui::GetScrollY() < ImGui::GetScrollMaxY()) {
			mWishScrollToBottom = false;
		} else { mWishScrollToBottom = true; }
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
				if (historySize <= 0) { break; }

				// ↑↓を押したら入力中だったテキストを scratch に保存
				if (!c.browsing) {
					c.scratch.assign(data->Buf, data->BufTextLen);
					c.browsing     = true;
					c.historyIndex = historySize; // 末尾（scratch位置）から開始
				}

				const int prevHistoryIndex = c.historyIndex;
				if (data->EventKey == ImGuiKey_UpArrow) {
					if (c.historyIndex > 0) { c.historyIndex--; }
				} else if (data->EventKey == ImGuiKey_DownArrow) {
					if (c.historyIndex < historySize) {
						c.historyIndex++;
					} else { c.historyIndex = historySize; }
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

	size_t ConsoleUI::FilteredToActualIndex(int filteredIndex) {
		int visibleIndex = 0;
		for (size_t i = 0; i < mConsoleSystem->GetLogBuffer().Size(); ++i) {
			const auto& buffer = mConsoleSystem->GetLogBuffer()[i];
			if (
				gConsoleUIData.currentFilterChannel == kChannelNone ||
				buffer.channel == gConsoleUIData.currentFilterChannel) {
				if (visibleIndex == filteredIndex) { return i; }
				visibleIndex++;
			}
		}
		return SIZE_MAX;
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
}
#endif
