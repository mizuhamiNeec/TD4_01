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
		ImGuiWindowFlags_NoScrollbar;

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

			static ConsoleLogText selection;
			static bool           hasSelection = false;

			if (
				ImGui::BeginTable(
					"Show##ConsoleUI", 2,
					ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
					ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit,
					childSize
				)
			) {
				// ヘッダーを一番上に固定
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn(
					"Log", ImGuiTableColumnFlags_WidthStretch
				);
				ImGui::TableSetupColumn(
					"Channel", ImGuiTableColumnFlags_WidthFixed
				);
				ImGui::TableHeadersRow();

				// フィルタ後のインデックス用
				int visibleIndex = 0;

				gConsoleUIData.selected.resize(
					mConsoleSystem->GetLogBuffer().Size(), false
				);

				for (
					auto it = mConsoleSystem->GetLogBuffer().begin();
					it != mConsoleSystem->GetLogBuffer().end();
					++it
				) {
					const auto& buffer = *it;

					// フィルターに一致しない場合はスキップ
					if (gConsoleUIData.currentFilterChannel != kChannelNone &&
					    buffer.channel != gConsoleUIData.currentFilterChannel) {
						continue;
					}

					std::string display = buffer.message;
					display += "##" + std::to_string(it.Index()); // ユニーク ID 用

					ImGui::TableNextRow();

					// ログメッセージの列
					ImGui::TableSetColumnIndex(0);

					// テキストの色を設定
					PushTextColor(buffer);

					// セレクション用のユニーク ID は別で付与する
					ImGui::PushID(static_cast<int>(it.Index()));
					bool selected = gConsoleUIData.selected[it.Index()];
					if (ImGui::Selectable(display.c_str(), selected)) {
						if (ImGui::GetIO().KeyCtrl) {
							gConsoleUIData.selected[it.Index()] =
								!gConsoleUIData.selected[it.Index()];
						} else if (
							ImGui::GetIO().KeyShift &&
							gConsoleUIData.lastSelectedIndex != -1
						) {
							// フィルタリング後の範囲選択
							const int start = std::min(
								gConsoleUIData.lastSelectedIndex, visibleIndex
							);
							const int end = std::max(
								gConsoleUIData.lastSelectedIndex, visibleIndex
							);
							for (int j = start; j <= end; ++j) {
								const size_t actualIndex =
									FilteredToActualIndex(j);
								if (actualIndex != SIZE_MAX) {
									gConsoleUIData.selected[actualIndex] = true;
								}
							}
						} else {
							std::ranges::fill(gConsoleUIData.selected, false);
							gConsoleUIData.selected[it.Index()] = true;
						}

						selection    = buffer;
						hasSelection = true;
					}
					ImGui::PopID();
					ImGui::PopStyleColor();

					if (buffer.channel != kChannelNone) {
						if (ImGui::TableSetColumnIndex(1)) {
							std::string channelName = buffer.channel;
							if (!channelName.empty()) {
								if (ImGui::TextLink(
									(channelName + "##" +
									 std::to_string(it.Index())
									).c_str()
								)) {
									// チャンネルをクリックした時のフィルター設定
									if (gConsoleUIData.currentFilterChannel ==
									    buffer.channel) {
										// 同じチャンネルをクリックするとフィルター解除
										gConsoleUIData.currentFilterChannel =
											kChannelNone;
									} else {
										gConsoleUIData.currentFilterChannel =
											buffer.channel;
									}
								}
							}
						}
					} else { ImGui::TableSetColumnIndex(1); }

					visibleIndex++;
				}

				ShowContextMenu();

				if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
					std::string      copiedText;
					constexpr size_t kMaxCopyLength = 1024 * 1024; // 1MB
					for (size_t i = 0; i < mConsoleSystem->GetLogBuffer().Size()
					     ; ++i) {
						if (gConsoleUIData.selected[i]) {
							const auto& buffer =
								mConsoleSystem->GetLogBuffer()[i];
							if (copiedText.size() + buffer.message.size()
							    > kMaxCopyLength) {
								copiedText += "\n... (copy truncated) ...";
								break;
							}
							copiedText += buffer.message;
						}
					}
					ImGui::SetClipboardText(copiedText.c_str());
				}

				CheckScroll();
				ImGui::EndTable();
			}

			ImGui::Begin("About");
			const std::string bufferInfo = hasSelection ?
				                               std::format(
					                               "File: {}\nLine: {}\nColumn: {}\nFunc: {}",
					                               selection.location.
					                               file_name(),
					                               selection.location.line(),
					                               selection.location.column(),
					                               selection.location.
					                               function_name()
				                               ) :
				                               "(no selection)";

			ImGui::TextWrapped("%s", bufferInfo.c_str());
			const std::string command = hasSelection ?
				                            std::format(
					                            "--line {} --column {} {}",
					                            selection.location.line(),
					                            selection.location.column(),
					                            selection.location.file_name()
				                            ) :
				                            std::string{};

			if (hasSelection) {
				ImGui::Text("%s", command.c_str());
				if (
					ImGuiWidgets::IconButton(
						StrUtil::ConvertToUtf8(kIconBox).c_str(),
						"Open in Rider", ImVec2(128.0f, 32.0f), 1.0f,
						ImGuiDir_Right
					)
				) {
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
			ImGui::End();

			DrawInputTextAndSubmitButton();
		}

		ImGui::End();
	}

	void ConsoleUI::OnConsoleUpdate() { mWishScrollToBottom = true; }

	void ConsoleUI::ShowMenuBar() {
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
						StrUtil::ConvertToUtf8(kIconAbout).c_str(),
					)
					) {
					
				}
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
		if (ImGui::BeginPopupContextWindow()) {
			ImGui::Spacing();

			if (
				ImGuiWidgets::MenuItemWithIcon(
					StrUtil::ConvertToUtf8(kIconCopy).c_str(), " Copy Selected"
				)
			) {
				std::string      copiedText;
				constexpr size_t kMaxCopyLength = 1024 * 1024; // 1MB
				for (size_t i = 0; i < mConsoleSystem->GetLogBuffer().Size(); ++
				     i) {
					if (gConsoleUIData.selected[i]) {
						const auto& buffer = mConsoleSystem->GetLogBuffer()[i];
						if (copiedText.size() + buffer.message.size()
						    > kMaxCopyLength) {
							copiedText += "\n... (copy truncated) ...";
							break;
						}
						copiedText += buffer.message;
					}
					ImGui::SetClipboardText(copiedText.c_str());
				}
			}

			ImGui::Separator();

			if (
				ImGuiWidgets::MenuItemWithIcon(
					StrUtil::ConvertToUtf8(kIconVisibility).c_str(),
					gConsoleUIData.currentFilterChannel == kChannelNone ?
						" Show All Channels" :
						" Show All Channels (Clear Filter)"
				)
			) { gConsoleUIData.currentFilterChannel = kChannelNone; }

			ImGui::Spacing();

			ImGui::EndPopup();
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
}
#endif
