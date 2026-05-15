#pragma once
#include <string>
#include <unordered_set>
#include <vector>

#include "../base/BaseComponent.h"

namespace Unnamed {
	/// @brief 指定タグのエンティティが円柱範囲に入ったかを判定するトリガーコンポーネントです。
	class CylinderTriggerComponent final : public BaseComponent {
	public:
		/// @brief 毎フレーム判定を実行します。
		void OnTick(float deltaTime) override;

		/// @brief トリガーのTickグループを取得します。
		[[nodiscard]] TICK_GROUP GetTickGroup() const override;

		/// @brief コンポーネントの安定名を取得します。
		[[nodiscard]] std::string_view GetStableName() const override;

		/// @brief コンポーネント表示名を取得します。
		[[nodiscard]] std::string_view GetComponentName() const override;

#ifdef _DEBUG
		/// @brief Inspector 用の編集UIを描画します。
		void DrawInspectorImGui() override;
#endif

		/// @brief JSON から値を読み込みます。
		void Deserialize(const JsonReader& reader) override;

		/// @brief JSON へ値を書き込みます。
		void Serialize(JsonWriter& writer) const override;

	private:
		void RunDetection();
		void ExecuteEntityActivationActions(
			const std::vector<uint64_t>& targetGuids,
			bool                         activeState,
			std::string_view             actionName
		) const;
		void EmitTransitionLogs(
			const std::unordered_set<uint64_t>& currentDetected
		) const;

		float       mRadius          = 3.0f;
		float       mHeight          = 2.0f;
		std::string mTargetTag       = "Ball";
		bool        mDebugLogEnabled = false;
		std::vector<uint64_t> mEnterActivateEntityGuids;
		std::vector<uint64_t> mExitDeactivateEntityGuids;

		std::unordered_set<uint64_t> mPreviousDetectedGuids;
	};
}
