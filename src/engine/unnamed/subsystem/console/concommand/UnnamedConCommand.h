#pragma once
#include <functional>

#include <engine/unnamed/subsystem/console/concommand/base/UnnamedConCommandBase.h>

namespace Unnamed {
	/// @brief 名前なしコンソールコマンドクラス
	class UnnamedConCommand : public UnnamedConCommandBase {
	public:
		// コールバック[引数: トークン化されたコマンド]
		using OnExecute  = std::function<bool(std::vector<std::string>)>;
		using OnComplete = std::function<void()>;

		UnnamedConCommand(
			const std::string_view& name,
			OnExecute               callback,
			const std::string_view& description,
			const FCVAR&            flags      = FCVAR::NONE,
			OnComplete              onComplete = {}
		);

		[[nodiscard]] bool IsCommand() const override;

		OnExecute  onExecute;  // コマンドを実行した際に呼び出されるコールバック
		OnComplete onComplete; // コマンドの実行が完了した際に呼び出されるコールバック

	private:
		void RegisterSelf();
	};
}
