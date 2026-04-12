#pragma once
#include "base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	class BoxKinematicCollisionResolver final : public
		BaseKinematicCollisionResolver {
	public:
		using BaseKinematicCollisionResolver::BaseKinematicCollisionResolver;

		/// @brief 衝突判定用のボックスを更新します。
		void UpdateHull(
			const Vec3& pos, const Vec3& halfExtents
		);

		/// @brief スライド移動を実行します。
		void SlideMove(
			Vec3& position, Vec3& velocity, float timeTotal
		) const override;

		/// @brief 段差移動を実行します。
		void StepMove(
			Vec3& position, Vec3& velocity, float stepHeight, float timeTotal
		) const override;

		/// @brief 接地判定を行います。
		bool ProbeGround(
			const Vec3& position, float maxDistance, Physics::Hit* outHit
		) const override;

		/// @brief 指定位置での重なり候補を収集します。
		int CollectOverlaps(
			const Vec3& position,
			const Vec3& halfExtents,
			Physics::Hit* outHits,
			int           maxHits
		) const override;

	private:
		static float CastSkinM();
		/// @brief 接触判定で使うスキン幅を返します。
		static float SkinM();

		Box mHull;
	};
}
