#pragma once

#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <Windows.h>
#include <shellapi.h>

namespace Unnamed {
	/// @brief App 起動引数から抽出した共通オプションです。
	struct AppLaunchOptions {
		/// @brief `--game` で指定されたゲーム名です。
		std::optional<std::string> gameName = std::nullopt;
		/// @brief `--repo-root` で指定された repo root です。
		std::optional<std::filesystem::path> repoRootOverride = std::nullopt;
		/// @brief `--help` / `-h` が指定されたかどうかです。
		bool showHelp = false;
		/// @brief 起動引数診断（警告/エラー）です。
		std::vector<std::string> diagnostics = {};
	};

	/// @brief ワイド文字列を UTF-8 文字列へ変換します。
	[[nodiscard]] inline std::string ConvertWideToUtf8(
		const std::wstring_view text
	) {
		if (text.empty()) {
			return {};
		}

		const int requiredSize = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			text.data(),
			static_cast<int>(text.size()),
			nullptr,
			0,
			nullptr,
			nullptr
		);
		if (requiredSize <= 0) {
			return {};
		}

		std::string output(static_cast<size_t>(requiredSize), '\0');
		const int writtenSize = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			text.data(),
			static_cast<int>(text.size()),
			output.data(),
			requiredSize,
			nullptr,
			nullptr
		);
		if (writtenSize != requiredSize) {
			return {};
		}
		return output;
	}

	/// @brief 起動前ログを標準エラーとデバッガ出力へ送ります。
	[[nodiscard]] inline std::string BuildLaunchMessage(
		const std::string_view appName,
		const std::string_view message
	) {
		std::string line = "[";
		line.append(appName);
		line.append("] ");
		line.append(message);
		return line;
	}

	/// @brief 起動前ログを出力します。
	inline void EmitPreLaunchLog(
		const std::string_view appName,
		const std::string_view message
	) {
		const std::string line = BuildLaunchMessage(appName, message);
		std::fputs((line + "\n").c_str(), stderr);
		::OutputDebugStringA((line + "\n").c_str());
	}

	/// @brief 起動オプションのヘルプを表示します。
	inline void PrintLaunchHelp(const std::string_view executableName) {
		std::string helpText;
		helpText += "Usage:\n";
		helpText += "  ";
		helpText += executableName;
		helpText += " [options]\n\n";
		helpText += "Options:\n";
		helpText += "  --help, -h               Show this help and exit.\n";
		helpText += "  --game=<name>            Select game profile by name or alias.\n";
		helpText += "  --game <name>            Select game profile by name or alias.\n";
		helpText += "  --repo-root=<path>       Explicit repository root for manifest search.\n";
		helpText += "  --repo-root <path>       Explicit repository root for manifest search.\n\n";
		helpText += "Environment:\n";
		helpText += "  UNNAMED_REPO_ROOT=<path> Explicit repository root for manifest search.\n\n";
		helpText += "Manifest search priority:\n";
		helpText += "  1) --repo-root\n";
		helpText += "  2) UNNAMED_REPO_ROOT\n";
		helpText += "  3) Upward search from current working directory\n";
		helpText += "  4) Upward search from executable directory\n\n";
		helpText += "Example:\n";
		helpText += "  UnnamedEditorApp.exe --game=TeamGame --repo-root=S:/Repositories/UnnamedEngine\n";

		std::fputs(helpText.c_str(), stdout);
		::OutputDebugStringA(helpText.c_str());
	}

	/// @brief 解析した引数診断を表示します。
	inline void EmitLaunchOptionDiagnostics(
		const std::string_view  appName,
		const AppLaunchOptions& options
	) {
		for (const std::string& diagnostic : options.diagnostics) {
			EmitPreLaunchLog(appName, diagnostic);
		}
	}

	/// @brief 現在プロセスのコマンドラインを共通ルールで解析します。
	/// @details `--game[= ]` と `--repo-root[= ]` と `--help/-h` に対応します。
	[[nodiscard]] inline AppLaunchOptions ParseAppLaunchOptionsFromCommandLine() {
		AppLaunchOptions options = {};
		const auto appendDiagnostic = [&](const std::string_view text) {
			options.diagnostics.emplace_back(text);
		};
		const auto isOptionToken = [](const std::wstring_view token) {
			return !token.empty() && token[0] == L'-';
		};
		const auto isEmptyOrWhitespace = [](const std::wstring_view text) {
			for (wchar_t ch : text) {
				if (!iswspace(static_cast<unsigned>(ch))) {
					return false;
				}
			}
			return true;
		};

		int argc = 0;
		LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
		if (argv == nullptr) {
			appendDiagnostic(
				"failed to parse command line arguments (CommandLineToArgvW returned null)"
			);
			return options;
		}

		for (int i = 1; i < argc; ++i) {
			const std::wstring_view arg(argv[i]);

			if (arg == L"--help" || arg == L"-h") {
				options.showHelp = true;
				continue;
			}

			if (arg.rfind(L"--game=", 0) == 0) {
				const std::wstring_view gameText = arg.substr(7);
				if (gameText.empty() || isEmptyOrWhitespace(gameText)) {
					appendDiagnostic("empty value for --game=...; option ignored");
					continue;
				}

				const std::string gameName = ConvertWideToUtf8(gameText);
				if (!gameName.empty()) {
					options.gameName = gameName;
				} else {
					appendDiagnostic(
						"failed to decode --game value to UTF-8; option ignored"
					);
				}
				continue;
			}

			if (arg == L"--game") {
				if (i + 1 >= argc || isOptionToken(argv[i + 1])) {
					appendDiagnostic(
						"missing value after --game; expected --game <name>"
					);
					continue;
				}
				const std::string gameName = ConvertWideToUtf8(argv[i + 1]);
				if (!gameName.empty()) {
					options.gameName = gameName;
				} else {
					appendDiagnostic(
						"failed to decode --game value to UTF-8; option ignored"
					);
				}
				++i;
				continue;
			}

			if (arg.rfind(L"--repo-root=", 0) == 0) {
				const std::wstring_view pathText = arg.substr(12);
				if (pathText.empty() || isEmptyOrWhitespace(pathText)) {
					appendDiagnostic("empty value for --repo-root=...; option ignored");
					continue;
				}

				options.repoRootOverride = std::filesystem::path(std::wstring(pathText));
				continue;
			}

			if (arg == L"--repo-root") {
				if (i + 1 >= argc || isOptionToken(argv[i + 1])) {
					appendDiagnostic(
						"missing value after --repo-root; expected --repo-root <path>"
					);
					continue;
				}
				options.repoRootOverride = std::filesystem::path(argv[i + 1]);
				++i;
				continue;
			}

			if (arg.rfind(L"--", 0) == 0) {
				appendDiagnostic(
					"unknown option '" + ConvertWideToUtf8(arg) + "'; option ignored"
				);
			}
		}

		if (options.repoRootOverride.has_value()) {
			std::error_code ec;
			const std::filesystem::path& repoRoot = *options.repoRootOverride;
			const bool exists = std::filesystem::exists(repoRoot, ec);
			if (ec || !exists) {
				appendDiagnostic(
					"--repo-root path does not exist or is inaccessible: '" +
					repoRoot.generic_string() +
					"' (manifest search will continue with fallback candidates)"
				);
			}
		}

		::LocalFree(argv);
		return options;
	}
}
