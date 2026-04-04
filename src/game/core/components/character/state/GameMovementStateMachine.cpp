#include "GameMovementStateMachine.h"

#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "GMSM";

	GameMovementStateMachine::GameMovementStateMachine()  = default;
	GameMovementStateMachine::~GameMovementStateMachine() = default;

	void GameMovementStateMachine::Init() {
		mConsole                   = ServiceLocator::Get<ConsoleSystem>();
		mHasReportedMissingConsole = false;
		if (!mConsole) {
			Error(
				kChannel,
				"コンソールシステムの取得に失敗しました。"
			);
		}
	}

	void GameMovementStateMachine::Tick(
		MovementContext& context, const float deltaTime
	) {
		if (!mConsole) {
			if (!mHasReportedMissingConsole) {
				Warning(
					kChannel,
					"ConsoleSystem未初期化のため状態更新をスキップしています。"
				);
				mHasReportedMissingConsole = true;
			}
			return;
		}

		if (mCurrentState) {
			mCurrentState->Tick(context, deltaTime);
			if (!context.requestedState.empty()) {
				ChangeState(context.requestedState);
				context.requestedState.clear();
			}
		}
	}

	void GameMovementStateMachine::ChangeState(const std::string& stateName) {
		if (!mConsole) {
			if (!mHasReportedMissingConsole) {
				Warning(
					kChannel,
					"ConsoleSystem未初期化のため状態遷移をスキップしています。"
				);
				mHasReportedMissingConsole = true;
			}
			return;
		}

		if (stateName.empty()) {
			Warning(
				kChannel,
				"空の状態名で遷移要求が来たため、処理をスキップしました。"
			);
			return;
		}

		if (
			mCurrentState &&
			mCurrentState->GetStateName() == stateName
		) {
			// 同じ状態に変更しようとした場合は何もしない
			return;
		}

		// 状態名に対応する状態が存在するか確認
		const auto it = mStates.find(stateName);

		if (it != mStates.end()) {
			// 現在の状態を終了
			if (mCurrentState) {
				mCurrentState->Exit();
			}
			// 新しい状態に切り替えて開始
			mCurrentState = it->second;
			mCurrentState->Enter(mConsole);
			return;
		}

		Warning(
			kChannel,
			"未登録の状態 '{}' への遷移要求を無視しました。",
			stateName
		);
	}

	void GameMovementStateMachine::AddState(
		const std::shared_ptr<IMovementState>& state
	) {
		if (state) {
			const std::string stateName(state->GetStateName());
			if (const auto it = mStates.find(stateName); it != mStates.end()) {
				Warning(
					kChannel,
					"状態 '{}' は既に登録済みのため、上書きします。",
					stateName
				);
			}
			mStates[stateName] = state;
		}
	}

	std::shared_ptr<IMovementState>
	GameMovementStateMachine::GetCurrentState() const {
		return mCurrentState;
	}
}
