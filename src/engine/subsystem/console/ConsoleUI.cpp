#ifdef _DEBUG
#include <pch.h>
#include <string>

#include <engine/subsystem/console/ConsoleUI.h>

#include <imgui.h>
#include <imgui_internal.h>

#include "engine/ImGui/Icons.h"
#include "engine/ImGui/ImGuiWidgets.h"

namespace Unnamed {
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
	) : mConsoleSystem(consoleSystem) {
#ifdef _DEBUG
		bool isNewArg = false;
		for (int i = 1; i < __argc; ++i) {
			// wWinMainの場合は__wargvらしい  へぇー(x512)
			if (__wargv[i] && std::wcscmp(__wargv[i], L"-new") == 0) {
				isNewArg = true;
				break;
			}
		}
		mIsImGuiInitialized = !isNewArg;
#else
		mIsImGuiInitialized = false;
#endif
	}

	/// @brief コンソールUIを表示します。
	/// @details ImGuiのコンテキスト内で呼び出し
	void ConsoleUI::Show() {
		if (mIsImGuiInitialized) {
			ImGui::Begin("Console##ConsoleUI", nullptr, kWindowFlags);

			constexpr ImGuiChildFlags childFlags =
				ImGuiChildFlags_ResizeX |
				ImGuiChildFlags_FrameStyle;

			// このウィンドウで使えるサイズを取得
			const auto region = ImGui::GetWindowContentRegionMax();

			// 子ウィンドウの高さを計算
			const float childHeight = region.y -
				ImGui::GetFrameHeightWithSpacing() * 2.0f;

			ImGui::BeginChild(
				"Output##ConsoleUI", {region.x * 0.5f, childHeight}, childFlags
			);

			static ConsoleLogText selection;

			for (
				auto it = mConsoleSystem->GetLogBuffer().begin();
				it != mConsoleSystem->GetLogBuffer().end();
				++it
			) {
				const auto& buffer = *it;
				std::string display;
				if (!buffer.channel.empty()) {
					display =
						"[" + buffer.channel + "] " + // チャンネル名
						buffer.message;               // メッセージ
				} else {
					display = buffer.message;
				}

				display += "##" + std::to_string(it.Index()); // ユニーク ID 用

				// テキストの色を設定
				PushTextColor(buffer);

				// セレクション用のユニーク ID は別で付与する
				ImGui::PushID(static_cast<int>(it.Index()));
				if (ImGui::Selectable(display.c_str(), false)) {
					selection = buffer;
				}
				ImGui::PopID();

				ImGui::PopStyleColor();
			}

			CheckScroll();

			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::BeginChild(
				"About##ConsoleUI", ImVec2(0, childHeight),
				ImGuiChildFlags_AlwaysUseWindowPadding |
				ImGuiChildFlags_FrameStyle
			);

			const std::string bufferInfo = std::format(
				"File: {}\nLine: {}\nColumn: {}\nFunc: {}",
				selection.location.file_name(),
				selection.location.line(),
				selection.location.column(),
				selection.location.function_name()
			);

			ImGui::TextWrapped(bufferInfo.data());
			const std::string command = std::format(
				"Rider.cmd --line {} --column {} {}",
				selection.location.line(),
				selection.location.column(),
				selection.location.file_name()
			);

			ImGui::Text(command.c_str());
			if (
				ImGuiWidgets::IconButton(
					StrUtil::ConvertToUtf8(kIconNANKABOX).c_str(),
					"Open in Rider"
				)
			) {
				system(command.c_str());
			}

			ImGui::EndChild();

			DrawInputText();

			ImGui::SameLine();

			DrawSubmitButton();

			ImGui::End();
		}
	}

	/// @brief コンソールが更新された際のイベント
	void ConsoleUI::OnConsoleUpdate() {
		mWishScrollToBottom = true;
	}

	/// @brief 入力テキストボックスを描画します。
	void ConsoleUI::DrawInputText() {
		const auto style        = ImGui::GetStyle();
		const auto spacing      = style.WindowPadding;
		const auto framePadding = style.FramePadding;
		const auto itemSpacing  = style.ItemSpacing;

		const auto submitTextWidth = ImGui::CalcTextSize(kSubmitButtonText);

		const ImVec2 size = {
			std::max(
				ImGui::GetWindowContentRegionMax().x - spacing.x -
				submitTextWidth.x - framePadding.x * 2.0f - itemSpacing.x,
				0.0f
			),
			ImGui::GetFrameHeightWithSpacing() - spacing.y
		};

		if (
			ImGui::InputTextEx(
				"##Input",
				nullptr,
				mInputBuffer,
				IM_ARRAYSIZE(mInputBuffer),
				size,
				kInputTextFlags,
				reinterpret_cast<ImGuiInputTextCallback>(InputTextCallback)
			)
		) {
			Submit();
		}
	}

	/// @brief 送信ボタンを描画します。
	void ConsoleUI::DrawSubmitButton() {
		if (ImGui::Button(kSubmitButtonText)) {
			Submit();
		}
	}

	/// @brief コマンドが送信された際のイベント
	void ConsoleUI::Submit() {
		// コンソールシステムに送る
		mConsoleSystem->ExecuteCommand(std::string(mInputBuffer));
		mInputBuffer[0] = '\0';

		// コマンドが送信されたらスクロールを一番下にする
		mWishScrollToBottom = true;
	}

	void ConsoleUI::CheckScroll() {
		if (
			mWishScrollToBottom &&
			ImGui::GetScrollY() >= ImGui::GetScrollMaxY()
		) {
			ImGui::SetScrollHereY(1.0f);
		}

		if (ImGui::GetScrollY() < ImGui::GetScrollMaxY()) {
			mWishScrollToBottom = false;
		} else {
			mWishScrollToBottom = true;
		}
	}

	/// @brief コンソールログのテキストの色を設定します。
	/// @param buffer コンソールログのテキスト情報
	void ConsoleUI::PushTextColor(const ConsoleLogText& buffer) {
		switch (buffer.level) {
		case LogLevel::None:
			ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(kConTextColor));
			break;
		case LogLevel::Info:
			ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(kConTextColor));
			break;
		case LogLevel::Dev:
			ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(kConTextColorDev));
			break;
		case LogLevel::Warning:
			ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(kConTextColorWarn));
			break;
		case LogLevel::Error:
			ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(kConTextColorError));
			break;
		case LogLevel::Fatal:
			ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(kConTextColorFatal));
			break;
		case LogLevel::Execute:
			ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(kConTextColorExec));
			break;
		case LogLevel::Waiting:
			ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(kConTextColorWait));
			break;
		case LogLevel::Success:
			ImGui::PushStyleColor(
				ImGuiCol_Text, ToImVec4(kConTextColorSuccess)
			);
			break;
		}
	}

	/// @brief インプットテキストからのコールバック
	int ConsoleUI::InputTextCallback(const ImGuiInputTextCallbackData* data) {
		switch (data->EventFlag) {
		case ImGuiInputTextFlags_CallbackCompletion: {
			Msg("callback", "completion");
		}
		break;

		case ImGuiInputTextFlags_CallbackHistory:
			Msg("callback", "history");
			break;

		case ImGuiInputTextFlags_CallbackEdit:
			Msg("callback", "edit");
			break;

		case ImGuiInputTextFlags_CallbackResize:
			break;
		default: ;
		}
		return 0;
	}
}
#endif
