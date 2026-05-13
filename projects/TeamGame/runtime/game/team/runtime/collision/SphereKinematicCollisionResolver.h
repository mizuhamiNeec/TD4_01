#pragma once
#include "base/BaseKinematicCollisionResolver.h"

namespace Unnamed {
	/// @brief 球体用のキネマティック衝突解決器です。
	class SphereKinematicCollisionResolver final : public
		BaseKinematicCollisionResolver {
	public:
		SphereKinematicCollisionResolver() = delete;

		explicit SphereKinematicCollisionResolver(Physics::Engine* engine)
			: BaseKinematicCollisionResolver(engine) {}

		/// @brief 衝突判定用の球を更新します。
		/// @param position 球の中心位置
		/// @param radius 球の半径 [m]
		void UpdateHull(const Vec3& position, float radius);

		/// @brief 衝突を考慮して移動を行います。
		void SlideMove(
			Vec3& position, Vec3& velocity, float timeTotal
		) const override;

		/// @brief 段差移動を行います。
		void StepMove(
			Vec3& position, Vec3& velocity, float stepHeightHu, float timeTotal
		) const override;

		/// @brief 指定位置の直下に支持面があるかを調べます。
		bool ProbeGround(
			const Vec3& position, float maxDistance, Physics::Hit* outHit
		) const override;

		/// @brief 指定位置での重なりを収集します。
		int CollectOverlaps(
			const Vec3& position, const Vec3& halfExtents,
			Physics::Hit* outHits, int maxHits
		) const override;

	private:
		/// @brief 指定位置での重なりを検査します。
		[[nodiscard]] bool IsHullOverlapping(
			const Vec3& position,
			Physics::Hit* outHit = nullptr
		) const;

		/// @brief 開始重なり時に押し出しと近傍探索で復帰を試みます。
		[[nodiscard]] bool RecoverFromPenetration(
			Vec3& position,
			const Vec3& preferredDirection
		) const;

		/// @brief 接触判定で使うスキン幅を返します。
		static float SkinM();
		
		/// @brief 接触判定用の球です。
		Sphere mHull = {};
	};
}
