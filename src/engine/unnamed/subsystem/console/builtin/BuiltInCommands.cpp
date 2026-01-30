#include "BuiltInCommands.h"

#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/concommand/UnnamedConCommand.h>
#include <engine/unnamed/subsystem/console/concommand/UnnamedConVar.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>

#include "engine/unnamed/subsystem/console/ConsoleScriptParser.h"

namespace Unnamed {
	void RegisterBuiltInCommands() {
		// 数値のトグル処理（値リストから現在値を探し、次の値へ）
		auto ToggleSequence = [](
			auto*                           var,
			const std::vector<std::string>& values,
			auto                            parse
		) {
			if (!var || values.empty()) return;
			using T         = decltype(var->GetValue());
			const T current = static_cast<T>(*var);

			for (size_t i = 0; i < values.size(); ++i) {
				if (current == parse(values[i])) {
					const size_t nextIndex = (i + 1) % values.size();
					var->SetValue(parse(values[nextIndex]));
					Msg(kChannelNone, "Toggle: {}", var->GetValue());
					return;
				}
			}

			// 見つからなければ最初の値へ
			var->SetValue(parse(values[0]));
			Msg(kChannelNone, "Toggle: {}", var->GetValue());
		};

		static UnnamedConCommand toggle(
			"toggle",
			[ToggleSequence](std::vector<std::string> args) {
				// 引数がなければ何もせず終了。
				if (args.empty()) { return false; }

				auto* console = ServiceLocator::Get<ConsoleSystem>();
				// 最初の引数がCVar名、その後が切り替えたい値のリスト
				const std::vector<std::string> argValues(
					args.begin() + 1, args.end()
				);
				const auto argSize = argValues.size();            // 値の数
				const auto var     = console->GetConVar(args[0]); // CVarを取得

				switch (GetConVarType(var)) {
					case CVAR_TYPE::NONE: break;

					case CVAR_TYPE::BOOL: {
						const auto bVar = dynamic_cast<UnnamedConVar<bool>*>(
							var);
						bool bValue = static_cast<bool>(*bVar);
						bVar->SetValue(!bValue);
						Msg(kChannelNone, "Toggle: {}", bVar->GetValue());
					}
					break;

					case CVAR_TYPE::INT: {
						if (argSize == 0) return false;
						auto* iVar = dynamic_cast<UnnamedConVar<int>*>(var);
						ToggleSequence(
							iVar, argValues, [](const std::string& s) {
								return std::stoi(s);
							}
						);
					}
					break;

					case CVAR_TYPE::FLOAT: {
						if (argSize == 0) return false;
						auto* fVar = dynamic_cast<UnnamedConVar<float>*>(var);
						ToggleSequence(
							fVar, argValues, [](const std::string& s) {
								return std::stof(s);
							}
						);
					}
					break;

					case CVAR_TYPE::DOUBLE: {
						if (argSize == 0) return false;
						auto* dVar = dynamic_cast<UnnamedConVar<double>*>(var);
						ToggleSequence(
							dVar, argValues, [](const std::string& s) {
								return std::stod(s);
							}
						);
					}
					break;

					case CVAR_TYPE::STRING: {
						if (argSize == 0) return false;
						auto* sVar = dynamic_cast<UnnamedConVar<std::string>*>(
							var);
						ToggleSequence(
							sVar, argValues, [](const std::string& s) {
								return s;
							}
						);
					}
					break;
					case CVAR_TYPE::VEC3:
						// 使う...か?
						break;
				}
				return true;
			},
			"Usage: toggle <cvar> <value 1> <value 2> <value 3> ..."
		);

		static UnnamedConCommand exec(
			"exec",
			[](const std::vector<std::string>& args) {
				if (args.empty()) {
					Error(kChannelNone, "Usage: exec <script file path>");
					return false;
				}

				ConsoleScriptParser parser(args[0]);
				return true;
			},
			"Execute a console script file."
		);
	}
}
