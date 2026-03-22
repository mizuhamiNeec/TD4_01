#include "GameMovementStateMachine.h"

#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	GameMovementStateMachine::GameMovementStateMachine()  = default;
	GameMovementStateMachine::~GameMovementStateMachine() = default;

	void GameMovementStateMachine::Init() {
		mConsole = ServiceLocator::Get<ConsoleSystem>();
		if (!mConsole) {
			Error(
				"GMSM",
				"コンソールシステムの取得に失敗しました。"
			);
		}
	}

	void GameMovementStateMachine::Tick(
		MovementContext& context, const float deltaTime
	) {
		if (mCurrentState) {
			mCurrentState->Tick(context, deltaTime);
			if (!context.requestedState.empty()) {
				ChangeState(context.requestedState);
				context.requestedState.clear();
			}
		}
	}

	void GameMovementStateMachine::ChangeState(std::string stateName) {
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
		}
	}

	void GameMovementStateMachine::AddState(
		const std::shared_ptr<IMovementState>& state
	) {
		if (state) {
			mStates[std::string(state->GetStateName())] = state;
		}
	}

	std::shared_ptr<IMovementState>
	GameMovementStateMachine::GetCurrentState() const {
		return mCurrentState;
	}
}
