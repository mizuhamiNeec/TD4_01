#pragma once

#include <memory>
#include <source_location>
#include <unordered_map>

#include <core/unnamed/containers/RingBuffer.h>

#include <engine/unnamed/subsystem/console/ConsoleUI.h>
#include <engine/unnamed/subsystem/console/concommand/base/UnnamedConCommandBase.h>
#include <engine/unnamed/subsystem/console/interface/IConsole.h>
#include <engine/unnamed/subsystem/interface/ISubsystem.h>
#include <engine/unnamed/time/DateTime.h>

namespace Unnamed {
	class UnnamedConCommandBase;
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

	EXEC_FLAG operator|=(EXEC_FLAG& lhs, const EXEC_FLAG& rhs);
	bool      operator&(EXEC_FLAG lhs, EXEC_FLAG rhs);

	/// @brief コンソールシステムクラス
	/// @details Source風コンソールシステム
	class ConsoleSystem final : public ISubsystem, public IConsole {
	public:
		~ConsoleSystem() override;

		// ISubsystem
		bool Init() override;                  /// @brief 初期化
		void Update(float deltaTime) override; /// @brief 更新
		void Shutdown() override;              /// @brief シャットダウン

		/// @brief サブシステム名を取得します
		[[nodiscard]] const std::string_view GetName() const override;

		// IConsole
		/// @brief ログバッファを取得します
		[[nodiscard]]
		RingBuffer<ConsoleLogText, kConsoleBufferSize>& GetLogBuffer();

		/// @brief コンソールにログを出力します
		/// @details Logマクロから呼び出されます。というかLogマクロ専用です。
		/// @param level ログレベル
		/// @param channel チャンネル名
		/// @param message メッセージ
		/// @param location ソースコードの位置情報
		void Print(
			LogLevel         level, std::string_view       channel,
			std::string_view message, std::source_location location
		) override;

		/// @brief コンソールコマンドを登録します
		/// @param conCommand 登録するコマンドへのポインタ
		void RegisterConCommand(UnnamedConCommandBase* conCommand);

		/// @brief コンソール変数を登録します
		/// @param conVar 登録する変数へのポインタ
		void RegisterConVar(UnnamedConCommandBase* conVar);

		/// @brief コンソールにコマンドを送信します。
		/// @param command コマンド文字列
		/// @param flag フラグ
		void ExecuteCommand(
			const std::string& command,
			EXEC_FLAG          flag = EXEC_FLAG::FROM_ENGINE
		);

		/// @brief 登録されている全てのコンソールコマンドを取得します
		[[nodiscard]]
		std::unordered_map<std::string, UnnamedConCommandBase*> GetConVars();

		/// @brief 名前からコンソール変数を取得します
		/// @param name 変数名
		/// @return 変数へのポインタ（存在しない場合はnullptr）
		UnnamedConCommandBase* GetConVar(std::string_view name);

		/// @brief UnnamedConCommandBaseからCVAR_TYPEを取得します
		/// @param var 変数へのポインタ
		/// @return 変数の型
		[[nodiscard]]
		static enum class CVAR_TYPE GetConVarType(UnnamedConCommandBase* var);

		/// @brief 指定した型のコンソール変数を名前から取得します
		/// @tparam TVar 取得する変数の型
		/// @param name 変数名
		/// @return 変数へのポインタ（存在しない場合や型が異なる場合はnullptr）
		template <class TVar>
		[[nodiscard]] TVar* GetConVarAs(const std::string_view name) {
			static_assert(
				std::is_base_of_v<UnnamedConCommandBase, TVar>,
				"TVar must be derived from UnnamedConCommandBase"
			);

			auto* base = GetConVar(name);
			if (!base) { return nullptr; }

			if (GetConVarType(base) != TVar::kType) { return nullptr; }

			return static_cast<TVar*>(base);
		}

	private:
		/// @brief 一般のコマンドを登録します
		void RegisterCommonCommands();

		RingBuffer<ConsoleLogText, kConsoleBufferSize> mLogBuffer;

		std::unordered_map<std::string, UnnamedConCommandBase*> mConCommands;
		std::unordered_map<std::string, UnnamedConCommandBase*> mConVars;

#ifdef _DEBUG // デバッグ時にはコンソールUIを有効化
		std::unique_ptr<ConsoleUI> mConsoleUI;
#endif
	};
}
