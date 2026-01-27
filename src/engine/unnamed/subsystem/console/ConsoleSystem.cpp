#include <pch.h>
//-----------------------------------------------------------------------------
#include <iostream>

#include <engine/OldConsole/Console.h>
#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/Log.h>
#include <engine/unnamed/subsystem/console/builtin/BuiltInCommands.h>
#include <engine/unnamed/subsystem/console/builtin/BuiltInConVars.h>
#include <engine/unnamed/subsystem/console/concommand/UnnamedConCommand.h>
#include <engine/unnamed/subsystem/console/concommand/UnnamedConVar.h>
#include <engine/unnamed/subsystem/console/concommand/base/UnnamedConCommandBase.h>
#include <engine/unnamed/subsystem/console/concommand/base/UnnamedConVarBase.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>
#include <engine/unnamed/subsystem/time/SystemClock.h>

#include "ConsoleScriptParser.h"
#include "ConVarWriter.h"

namespace Unnamed {
	static constexpr std::string_view kChannel     = "Console";
	static constexpr std::string_view kUserCfgPath =
		"./content/core/cfg/user.cfg";

	/// @brief EXEC_FLAGのOR演算子オーバーロード
	/// @param lhs 左辺
	/// @param rhs 右辺
	/// @return 演算結果
	EXEC_FLAG operator|=(EXEC_FLAG& lhs, const EXEC_FLAG& rhs) {
		lhs = static_cast<EXEC_FLAG>(static_cast<int>(lhs) | static_cast<int>(
			                             rhs));
		return lhs;
	}

	/// @brief EXEC_FLAGのAND演算子オーバーロード
	/// @param lhs 左辺
	/// @param rhs 右辺
	/// @return 演算結果
	bool operator&(EXEC_FLAG lhs, EXEC_FLAG rhs) {
		return (static_cast<int>(lhs) & static_cast<int>(rhs)) != 0;
	}

	/// @brief コンソールシステムのデストラクタ
	ConsoleSystem::~ConsoleSystem() = default;

	/// @brief コンソールシステムの初期化
	/// @return 初期化成功ならtrue
	bool ConsoleSystem::Init() {
		ServiceLocator::Register<ConsoleSystem>(this);

#ifdef _DEBUG
		mConsoleUI = std::make_unique<ConsoleUI>(this);
		mConsoleUI->Init();
#endif

		RegisterBuiltInCommands();
		RegisterBuiltInConVars();

		ConsoleScriptParser parser(kUserCfgPath);

		return true;
	}

	/// @brief コンソールシステムの更新
	/// @param deltaTime 前フレームからの経過時間（秒）
	void ConsoleSystem::Update(float) {
#ifdef _DEBUG
		mConsoleUI->Show();
#endif
	}

	/// @brief コンソールシステムのシャットダウン
	void ConsoleSystem::Shutdown() {
		ConVarWriter writer(kUserCfgPath);
	}

	/// @brief コンソールシステムの名前を取得します
	/// @return コンソールシステムの名前
	const std::string_view ConsoleSystem::GetName() const {
		return "Console";
	}

	RingBuffer<ConsoleLogText, kConsoleBufferSize>&
	ConsoleSystem::GetLogBuffer() {
		return mLogBuffer;
	}

	/// @brief コンソールにログを出力します
	/// @param level ログレベル
	/// @param channel チャンネル名
	/// @param message メッセージ
	/// @param location ソースコードの位置情報
	void ConsoleSystem::Print(
		const LogLevel             level,
		const std::string_view     channel,
		const std::string_view     message,
		const std::source_location location
	) {
		// ログメッセージをバッファに追加
		ConsoleLogText logText;
		logText.level     = level;
		logText.channel   = channel;
		logText.message   = message;
		logText.timeStamp = SystemClock::GetDateTime(SystemClock::Now());
		logText.location  = location;
		mLogBuffer.Push(logText);

		std::string out;
		if (!logText.channel.empty()) {
			out = "[" + logText.channel + "] " + logText.message;
		} else {
			out = logText.message;
		}

		// コンソールの出力
		std::cout << out << "\n";

		// Visual Studioの出力
		OutputDebugStringW(StrUtil::ToWString(out).c_str());
		OutputDebugStringW(StrUtil::ToWString("\n").c_str());

#ifdef _DEBUG
		mConsoleUI->OnConsoleUpdate();
#endif
	}

	/// @brief コンソールコマンドを登録します
	/// @param conCommand 登録するコマンドへのポインタ
	void ConsoleSystem::RegisterConCommand(UnnamedConCommandBase* conCommand) {
		mConCommands[std::string(conCommand->GetName())] = conCommand;
	}

	/// @brief コンソール変数を登録します
	/// @param conVar 登録する変数へのポインタ
	void ConsoleSystem::RegisterConVar(UnnamedConVarBase* conVar) {
		mConVars[std::string(conVar->GetName())] = conVar;
	}

	/// @brief コンソールにコマンドを送信します。
	/// @param command コマンド文字列
	/// @param flag フラグ
	void ConsoleSystem::ExecuteCommand(
		const std::string& command,
		const EXEC_FLAG    flag
	) {
		// 空なので何もしない
		if (command.empty()) {
			SpecialMsg(
				LogLevel::Execute,
				"",
				" ]"
			);
			return;
		}

		std::string trimmed = StrUtil::TrimSpaces(std::string(command));

		// コマンドを区切る
		auto commands = StrUtil::SplitCommands(trimmed);

		if (flag & EXEC_FLAG::SILENT) {
			// TODO 
		} else {
			// 送信内容をコンソールに表示
			SpecialMsg(
				LogLevel::Execute,
				"",
				"> {}",
				trimmed
			);
#ifdef _DEBUG
			mConsoleUI->OnConsoleUpdate();
#endif
		}

		for (const auto& singleCommand : commands) {
			if (singleCommand.empty()) {
				continue;
			}

			std::vector<std::string> tokens = StrUtil::Tokenize(singleCommand);
			const std::vector        args(tokens.begin() + 1, tokens.end());

			const bool foundCommand = mConCommands.contains(tokens[0]);
			const bool foundVar     = mConVars.contains(tokens[0]);

			// コマンドの場合
			if (foundCommand) {
				const auto* cmd = reinterpret_cast<UnnamedConCommand*>(
					mConCommands[tokens[0]]
				);

				// コールバックを実行
				if (cmd->onExecute(args)) {
					// 実行が完了したら完了時のコールバックを呼ぶ
					if (cmd->onComplete) {
						cmd->onComplete();
					}
				} else {
					// 失敗したらとりあえず説明を出しておく
					SpecialMsg(
						LogLevel::None,
						"",
						"{}: {}",
						cmd->GetName(),
						cmd->GetDescription()
					);
				}

				break;
			}

			// 変数の場合
			if (foundVar) {
				auto* var = mConVars[tokens[0]];

				// CVarの形を取得
				auto cvarType = GetConVarType(var);

				// 引数がある場合は値を設定
				if (!args.empty()) {
					switch (cvarType) {
						case CVAR_TYPE::BOOL: if (auto* cBool = dynamic_cast<
								UnnamedConVar<bool>*>(
								var)) {
								cBool->SetValue(
									StrUtil::CheckBoolString(args[0])
								);
							}
							break;
						case CVAR_TYPE::INT: if (
								auto* ci = dynamic_cast<UnnamedConVar<int>*>(
									var)
							) {
								ci->SetValue(std::stoi(args[0]));
							}
							break;
						case CVAR_TYPE::FLOAT: if (
								auto* cf = dynamic_cast<UnnamedConVar<float>*>(
									var)
							) {
								cf->SetValue(std::stof(args[0]));
							}
							break;
						case CVAR_TYPE::DOUBLE: if (
								auto* cd = dynamic_cast<UnnamedConVar<double>*>(
									var)
							) {
								cd->SetValue(std::stod(args[0]));
							}
							break;
						case CVAR_TYPE::STRING: if (
								auto* cs = dynamic_cast<UnnamedConVar<
										std::string>*>
									(var)
							) {
								// 空白対応
								std::string combined;
								for (size_t i = 0; i < args.size(); ++i) {
									combined += args[i];
									if (i != args.size() - 1) {
										combined += " ";
									}
								}
								cs->SetValue(combined);
							}
							break;
						case CVAR_TYPE::VEC3:
							// Vec3の場合は3つの引数が必要
							if (args.size() >= 3) {
								if (
									auto* cv3 = dynamic_cast<UnnamedConVar<Vec3>
										*>(
										var)
								) {
									bool ok = false;
									// 各引数が数字に変換できるか確認
									for (const auto& arg : args) {
										ok &= StrUtil::IsFloat(arg);
									}

									if (ok) {
										cv3->SetValue(
											Vec3(
												std::stof(args[0]),
												std::stof(args[1]),
												std::stof(args[2])
											)
										);
									}
								}
							}
							break;
					}
				}

				// if (var->onExecute(args)) {
				// }
			} else {
				// 謎な場合
				SpecialMsg(
					LogLevel::Error,
					"",
					"'{}' is not recognized as a command or variable.",
					tokens[0]
				);
			}
		}
	}

	std::unordered_map<std::string, UnnamedConVarBase*>
	ConsoleSystem::GetConVars() {
		return mConVars;
	}

	UnnamedConVarBase* ConsoleSystem::GetConVar(std::string_view name) {
		return mConVars.contains(name.data()) ?
			       mConVars[std::string(name)] :
			       nullptr;
	}

	CVAR_TYPE ConsoleSystem::GetConVarType(UnnamedConVarBase* var) {
		auto type = CVAR_TYPE::NONE;

		if (dynamic_cast<UnnamedConVar<bool>*>(var)) {
			type = CVAR_TYPE::BOOL;
		} else if (dynamic_cast<UnnamedConVar<int>*>(var)) {
			type = CVAR_TYPE::INT;
		} else if (dynamic_cast<UnnamedConVar<float>*>(var)) {
			type = CVAR_TYPE::FLOAT;
		} else if (dynamic_cast<UnnamedConVar<double>*>(var)) {
			type = CVAR_TYPE::DOUBLE;
		} else if (dynamic_cast<UnnamedConVar<std::string>*>(var)) {
			type = CVAR_TYPE::STRING;
		} else if (dynamic_cast<UnnamedConVar<Vec3>*>(var)) {
			type = CVAR_TYPE::VEC3;
		}

		return type;
	}
}
