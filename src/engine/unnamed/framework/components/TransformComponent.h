#pragma once
#include <vector>

#include "base/BaseComponent.h"

#include "core/math/Mat4.h"
#include "core/math/Quaternion.h"
#include "core/math/Vec3.h"

#include "engine/ImGui/Icons.h"

namespace Unnamed {
	class Scene;

	class TransformComponent : public BaseComponent {
	public:
		//---------------------------------------------------------------------
		// TransformComponent
		//---------------------------------------------------------------------

		[[nodiscard]] Vec3                GetPosition() const noexcept;
		[[nodiscard]] Quaternion          GetRotation() const noexcept;
		[[nodiscard]] Vec3                GetScale() const noexcept;
		[[nodiscard]] TransformComponent* GetParent() const noexcept;
		[[nodiscard]] const Mat4&         GetWorldMat() const noexcept;

		/// @brief 描画用のワールド行列を取得します。
		/// @details 固定ティック補間が有効な場合は補間後の姿勢を返します。
		[[nodiscard]] const Mat4& RenderWorldMat() const noexcept;

		void SetPosition(Vec3 position) noexcept;
		void SetRotation(Quaternion rotation) noexcept;
		void SetScale(Vec3 scale) noexcept;

		/// @brief 次の固定ティック境界で補間履歴を現在値へ再同期します。
		/// @details 物理シミュレーションのリセットや瞬間移動など、
		/// 補間が不自然になるような大きな変化があったときに呼び出します。
		void RequestInterpolationResync() noexcept;

		/// @brief 親を設定します。
		/// @param parent 新しい親の TransformComponent へのポインタ（nullptr で親なし）
		/// @param preserveWorld 親を変更してもワールド空間での位置を維持するかどうか（デフォルトは true）
		void SetParent(TransformComponent* parent, bool preserveWorld = true);

		/// @brief シーン内のエンティティ GUID から親を解決して設定します。
		/// @param scene 親を検索するシーン
		void ResolveDeferredParent(const Scene& scene);

		/// @brief ワールド右方向ベクトルを返します。
		[[nodiscard]] Vec3 Right() const noexcept;
		/// @brief ワールド上方向ベクトルを返します。
		[[nodiscard]] Vec3 Up() const noexcept;
		/// @brief ワールド前方向ベクトルを返します。
		[[nodiscard]] Vec3 Forward() const noexcept;

		//---------------------------------------------------------------------
		// BaseComponent
		//---------------------------------------------------------------------
		void OnAttached() override;
		void OnDetached() override;
		void OnTick(float deltaTime) override;

		/// @brief 描画フレームごとの補間係数を受け取り、描画行列生成に反映します。
		void OnRenderTick(
			float renderDeltaTime, float interpolationAlpha
		) override;

		void OnEditorTick(float deltaTime) override;

		[[nodiscard]] TICK_GROUP       GetTickGroup() const override;
		[[nodiscard]] std::string_view GetStableName() const override;
		[[nodiscard]] std::string_view GetComponentName() const override;
		[[nodiscard]] uint32_t         GetIcon() const override;

#ifdef _DEBUG
		void DrawInspectorImGui() override;
#endif

		void Deserialize(const JsonReader& reader) override;
		void Serialize(JsonWriter& writer) const override;

	private:
		/// @brief ローカル値からシミュレーション用ワールド行列を再帰的に更新します。
		void UpdateWorldRecursive();

		/// @brief 固定ティック境界で補間履歴（prev/current）を更新します。
		void RefreshInterpolationSamplesFromSimulation();

		/// @brief 描画補間を行うべきかを判断します。
		[[nodiscard]] bool ShouldInterpolateForRender() const;

		/// @brief 補間結果または現在値から描画ローカル行列を構築します。
		[[nodiscard]] Mat4 BuildRenderLocalMatrix() const;

		/// @brief 変更されたことをマークします。
		void MarkDirty();

		/// @brief シミュレーション後のオーバーライドがあることをマークします。
		void MarkPostSimulationOverride();

		Vec3       mLocalPos   = Vec3::zero;
		Quaternion mLocalRot   = Quaternion::identity;
		Vec3       mLocalScale = Vec3::one;

		Mat4         mWorldMat       = Mat4::identity;
		mutable Mat4 mRenderWorldMat = Mat4::identity;

		TransformComponent*              mParent = nullptr;
		std::vector<TransformComponent*> mChildren;

		uint64_t mPendingParentEntityGuid = 0;

		// 前回のトランスフォーム
		Vec3       mInterpPrevPos   = Vec3::zero;
		Quaternion mInterpPrevRot   = Quaternion::identity;
		Vec3       mInterpPrevScale = Vec3::one;

		// 現在のトランスフォーム
		Vec3       mInterpCurrPos   = Vec3::zero;
		Quaternion mInterpCurrRot   = Quaternion::identity;
		Vec3       mInterpCurrScale = Vec3::one;

		uint64_t mInterpSampleTick = 0; // 補間サンプルの固定ティックカウンタ

		float mRenderInterpAlpha = 1.0f; // 描画補間係数（0.0f - 1.0f）

		bool mInterpInitialized         = false; // 補間が初期化されているか?
		bool mInterpResyncRequested     = false; // 再同期要求があるか?
		bool mHasPostSimulationOverride = false; // シミュ後上書きがあるか?
		bool mIsDirty                   = false; // 変更されたか?
	};
}
