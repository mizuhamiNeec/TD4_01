#pragma once

#include <memory>
#include <source_location>
#include <unordered_map>

#include <core/containers/RingBuffer.h>

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

	/// @brief コンソールコマンド実行フラグ
	enum class EXEC_FLAG {
		NONE         = 0,
		SILENT       = 1 << 0,
		FROM_ENGINE  = 1 << 1,
		FROM_USER    = 1 << 2,
		FROM_CONSOLE = 1 << 3,
	};

	/// @brief EXEC_FLAGのOR演算子オーバーロード
	/// @param lhs 左辺
	/// @param rhs 右辺
	/// @return 演算結果
	EXEC_FLAG operator|=(EXEC_FLAG& lhs, const EXEC_FLAG& rhs);

	/// @brief EXEC_FLAGのAND演算子オーバーロード
	/// @param lhs 左辺
	/// @param rhs 右辺
	/// @return 演算結果
	bool operator&(EXEC_FLAG lhs, EXEC_FLAG rhs);

	/// @brief コンソールシステムクラス
	/// @details Source風多機能コンソールシステム。文字列によるコマンド実行や変数管理を行います。
	class ConsoleSystem final : public ISubsystem, public IConsole {
	public:
		/// @brief デストラクタ
		~ConsoleSystem() override;

		// ---ISubsystem---

		/// @brief コンソールシステムの初期化
		/// @return 初期化成功ならtrue
		bool Init() override;

		/// @brief 更新
		void Update(float deltaTime) override;

		/// @brief シャットダウン
		void Shutdown() override;

		/// @brief サブシステム名を取得します
		/// @return サブシステム名
		[[nodiscard]] const std::string_view GetName() const override;

		// ---IConsole---

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

			return static_cast<TVar*>(base);
		}

	private:
		/// @brief 一般コマンドを登録します
		void RegisterCommonCommands();

		// ログのリングバッファ
		RingBuffer<ConsoleLogText, kConsoleBufferSize> mLogBuffer;

		// 登録されているコマンドと変数のマップ
		std::unordered_map<std::string, UnnamedConCommandBase*> mConCommands;
		std::unordered_map<std::string, UnnamedConCommandBase*> mConVars;

#ifdef _DEBUG // デバッグ時(ImGui有効化時)にはコンソールUIを有効化
		std::unique_ptr<ConsoleUI> mConsoleUI;
#endif
	};
}
