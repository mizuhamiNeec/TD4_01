#pragma once

#include <memory>
#include <source_location>
#include <unordered_map>

#include <core/unnamed/containers/RingBuffer.h>

#include <engine/unnamed/subsystem/console/ConsoleUI.h>
#include <engine/unnamed/subsystem/console/concommand/base/UnnamedConVarBase.h>
#include <engine/unnamed/subsystem/console/interface/IConsole.h>
#include <engine/unnamed/subsystem/interface/ISubsystem.h>
#include <engine/unnamed/time/DateTime.h>

namespace Unnamed {
	class UnnamedConCommandBase;
	class UnnamedConVarBase;
	constexpr uint32_t kConsoleBufferSize = 1024;

	/// @brief コンソールログテキスト構造体
	struct ConsoleLogText {
		LogLevel             level;
		std::string          channel;
		std::string          message;
		DateTime             timeStamp;
		std::source_location location;
	};

	enum class EXEC_FLAG {
		NONE,
		SILENT,
		FROM_ENGINE,
		FROM_USER,
		FROM_CONSOLE,
	};

	EXEC_FLAG operator |=(EXEC_FLAG& lhs, const EXEC_FLAG& rhs);
	bool      operator&(EXEC_FLAG lhs, EXEC_FLAG rhs);

	/// @brief コンソールシステムクラス
	/// @details Source風コンソールシステム
	class ConsoleSystem final : public ISubsystem, public IConsole {
	public:
		~ConsoleSystem() override;

		// ISubsystem
		bool Init() override;
		void Update(float deltaTime) override;
		void Shutdown() override;

		[[nodiscard]] const std::string_view GetName() const override;

		// IConsole
		RingBuffer<ConsoleLogText, kConsoleBufferSize>& GetLogBuffer();

		void Print(
			LogLevel         level, std::string_view       channel,
			std::string_view message, std::source_location location
		) override;

		void RegisterConCommand(UnnamedConCommandBase* conCommand);

		void RegisterConVar(UnnamedConVarBase* conVar);

		void ExecuteCommand(
			const std::string& command,
			EXEC_FLAG          flag = EXEC_FLAG::FROM_ENGINE
		);

		[[nodiscard]]
		std::unordered_map<std::string, UnnamedConVarBase*> GetConVars();

		UnnamedConVarBase* GetConVar(std::string_view name);

		static enum class CVAR_TYPE GetConVarType(UnnamedConVarBase* var);

		void Test();

	private:
		RingBuffer<ConsoleLogText, kConsoleBufferSize> mLogBuffer;

		std::unordered_map<std::string, UnnamedConCommandBase*> mConCommands;
		std::unordered_map<std::string, UnnamedConVarBase*>     mConVars;

#ifdef _DEBUG // デバッグ時にはコンソールUIを有効化
		std::unique_ptr<ConsoleUI> mConsoleUI;
#endif
	};
}
