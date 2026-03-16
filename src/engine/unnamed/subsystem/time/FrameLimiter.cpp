#include "FrameLimiter.h"

#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/concommand/ConVar.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>

static constexpr std::string_view kChannel = "FrmLim";

/// @brief コンストラクタ
/// @param gameTime ゲームタイムクラスへのポインタ
FrameLimiter::FrameLimiter(GameTime* gameTime) :
	mGameTime(gameTime) {
	mConsoleSystem    = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	const auto fpsmax = mConsoleSystem->GetConVarAs<Unnamed::ConVar<
		double>>("fps_max");

	if (fpsmax) {
		SetTargetFPS(fpsmax->GetValue());
		DevMsg(kChannel, "Initial target FPS set to {}", fpsmax->GetValue());
	} else {
		// fps_maxが見つからない場合はデフォで無制限にする
		SetTargetFPS(0.0);
	}
}

/// @brief 目標FPSを設定します
/// @param targetFPS 目標FPS
void FrameLimiter::SetTargetFPS(const double targetFPS) {
	using namespace std::chrono;
	if (targetFPS > 0) {
		// 1/FPS 秒を duration に変換
		mTargetFrameDuration = duration_cast<Clock::duration>(
			duration<double>(1.0 / targetFPS)
		);
	} else {
		mTargetFrameDuration = Clock::duration::zero();
	}
}

/// @brief フレームの開始を記録します
void FrameLimiter::BeginFrame() {
	mFrameStart = Clock::now();
}

/// @brief フレームレートを制限します
void FrameLimiter::Limit() {
	CheckConVarValue();

	if (mTargetFrameDuration == Clock::duration::zero()) {
		return;
	}

	using namespace std::chrono;

	const auto now = Clock::now();

	const auto elapsed = now - mFrameStart;

	if (elapsed >= mTargetFrameDuration) {
		return;
	}

	const auto remaining = mTargetFrameDuration - elapsed;

	// 大まかにスリープする
	constexpr auto spinThreshold = milliseconds(10);
	if (remaining > spinThreshold) {
		std::this_thread::sleep_for(remaining - spinThreshold);
	}

	const auto endTime = mFrameStart + mTargetFrameDuration;
	while (Clock::now() < endTime) {
		std::this_thread::yield();
	}
}

/// @brief コンソール変数の値をチェックして目標FPSを更新します
void FrameLimiter::CheckConVarValue() {
	const auto fpsmax = mConsoleSystem->GetConVarAs<Unnamed::ConVar<
		double>>("fps_max");
	SetTargetFPS(fpsmax->GetValue());
}
