#include "SphereKinematicCollisionResolver.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/math/Math.h"

namespace Unnamed {
	namespace {
		constexpr float    kEpsilon = 1e-6f;
		constexpr float    kNormalEpsilon = 1e-12f;
		constexpr float    kDuplicatePlaneDot = 0.97f;
		constexpr float    kSkinHu = 1.0f;
		constexpr float    kOverbounce = 1.01f;
		constexpr uint32_t kMaxClipPlanes = 5;
		constexpr uint32_t kMaxSlideBump = 8;
		constexpr float    kMinConsumedFrac = 1e-5f;
		constexpr float    kZeroToiEpsilon = 1e-6f;
		constexpr float    kZeroToiConsumed = 0.05f;
		constexpr float    kZeroToiNonBlockingDot = 1e-4f;
		constexpr float    kTouchDepthRatio = 0.01f;
		constexpr float    kGroundNormalY = 0.7f;
		constexpr float    kStepSelectHorizEps = 1e-8f;
		constexpr float    kStepSelectDownEps = 1e-5f;
		constexpr uint32_t kMaxDepenetrationIters = 6;
		constexpr int      kDepenetrationHitBuffer = 64;
		constexpr float    kDepenetrationDepthEpsilon = 1e-5f;
		constexpr float    kDepenetrationMaxPushHu = 6.0f;

		[[nodiscard]] float HitDistanceM(
			const Physics::Hit& hit,
			const float         castLength
		) {
			return std::clamp(hit.toi, 0.0f, 1.0f) * std::max(0.0f, castLength);
		}

		[[nodiscard]] float HullRadiusM(const Sphere& hull) {
			return std::max(0.0f, hull.radius);
		}
	}

	void SphereKinematicCollisionResolver::UpdateHull(
		const Vec3& position, const float radius
	) {
		mHull = {
			.center = position,
			.radius = std::max(0.0f, radius)
		};
	}

	void SphereKinematicCollisionResolver::SlideMove(
		Vec3& position, Vec3& velocity, const float timeTotal
	) const {
		const float radius = HullRadiusM(mHull);
		if (radius <= kEpsilon) {
			velocity = Vec3::zero;
			return;
		}

		// 移動開始時の開始重なりを先に解消します。
		if (!RecoverFromPenetration(position, -velocity)) {
			velocity = Vec3::zero;
			return;
		}

		int         numPlanes = 0;
		float       timeLeft  = timeTotal;
		const float skin      = SkinM();
		const Vec3  primalVelocity = velocity;

		std::array<Vec3, kMaxClipPlanes> planes{};

		for (uint32_t bumpCount = 0; bumpCount < kMaxSlideBump; ++bumpCount) {
			if (velocity.SqrLength() < 1e-12f) {
				break;
			}

			// 現在速度から、残り時間で進める移動量を作成します。
			Vec3  move    = velocity * timeLeft;
			float moveLen = move.Length();
			if (moveLen < 1e-7f) {
				break;
			}

			Vec3 dir = move / moveLen;
			Physics::Hit hit{};
			const float  castLength = moveLen + skin;

			if (!mEngine->SphereCast(position, radius, dir, castLength, &hit)) {
				// sweep で見落とした終点めり込みを防ぐため、終点の重なりを再検査します。
				const Vec3 targetPos = position + move;
				if (IsHullOverlapping(targetPos, nullptr)) {
					Vec3 recoveredPos = targetPos;
					if (!RecoverFromPenetration(recoveredPos, -dir)) {
						velocity = Vec3::zero;
						break;
					}
					position = recoveredPos;
				} else {
					position = targetPos;
				}
				break;
			}

			if (hit.allsolid) {
				if (RecoverFromPenetration(position, -velocity)) {
					numPlanes = 0;
					continue;
				}
				velocity = Vec3::zero;
				return;
			}

			// hit.toi を実距離へ変換し、スキン幅を残して停止距離を決めます。
			const float hitDistance = HitDistanceM(hit, castLength);
			float       allowedDist = std::min(
				moveLen,
				std::max(0.0f, hitDistance - skin)
			);

			Vec3 normal = hit.normal;
			if (normal.SqrLength() > kNormalEpsilon) {
				normal.Normalize();
			} else {
				velocity = Vec3::zero;
				break;
			}

			const bool shallowStartSolid = hit.startSolid &&
			                               hit.depth <= skin * kTouchDepthRatio;
			const bool zeroToiContact =
				(!hit.startSolid || shallowStartSolid) &&
				hitDistance <= kZeroToiEpsilon &&
				allowedDist <= kEpsilon;
			const float moveIntoPlane = dir.Dot(normal);
			if (
				zeroToiContact &&
				moveIntoPlane > -kZeroToiNonBlockingDot
			) {
				// 非侵入のゼロTOI接触は進行阻害として扱わず、少し前進して次を探します。
				const float touchAdvance = std::min(moveLen, skin);
				if (touchAdvance > 1e-7f) {
					position += dir * touchAdvance;
				}
				float consumedFrac = std::clamp(
					touchAdvance / moveLen, 0.0f, 1.0f
				);
				consumedFrac = std::max(consumedFrac, kZeroToiConsumed);
				timeLeft    *= (1.0f - consumedFrac);
				if (timeLeft < 1e-7f) {
					break;
				}
				continue;
			}

			if (hit.startSolid && !shallowStartSolid) {
				if (!RecoverFromPenetration(position, -normal)) {
					velocity = Vec3::zero;
					return;
				}
				numPlanes = 0;
				continue;
			}

			if (allowedDist > 1e-7f) {
				position += dir * allowedDist;
				numPlanes = 0;
			}

			float consumedFrac = std::clamp(allowedDist / moveLen, 0.0f, 1.0f);
			consumedFrac       = std::max(
				consumedFrac,
				zeroToiContact ? kZeroToiConsumed : kMinConsumedFrac
			);
			timeLeft *= (1.0f - consumedFrac);
			if (timeLeft < 1e-7f) {
				break;
			}

			// 同一面の再ヒットを避けるため、法線を蓄積して滑走方向を更新します。
			bool duplicatePlane = false;
			for (int i = 0; i < numPlanes; ++i) {
				if (planes[i].Dot(normal) > kDuplicatePlaneDot) {
					velocity = ClipVelocity(velocity, normal, kOverbounce);
					duplicatePlane = true;
					break;
				}
			}
			if (duplicatePlane) {
				continue;
			}

			if (numPlanes >= kMaxClipPlanes) {
				velocity = Vec3::zero;
				break;
			}
			planes[numPlanes++] = normal;

			if (numPlanes == 1) {
				velocity = ClipVelocity(velocity, planes[0], kOverbounce);
			} else {
				Vec3 clipped;
				int  i;
				for (i = 0; i < numPlanes; ++i) {
					clipped = ClipVelocity(velocity, planes[i], kOverbounce);

					bool valid = true;
					for (int j = 0; j < numPlanes; ++j) {
						if (j == i) {
							continue;
						}
						if (clipped.Dot(planes[j]) < 0.0f) {
							valid = false;
							break;
						}
					}
					if (valid) {
						break;
					}
				}

				if (i == numPlanes) {
					if (numPlanes == 2) {
						Vec3  creaseDir = planes[0].Cross(planes[1]);
						float cLen      = creaseDir.Length();
						if (cLen > 1e-7f) {
							creaseDir = creaseDir / cLen;
							velocity   = creaseDir * velocity.Dot(creaseDir);
						} else {
							velocity = Vec3::zero;
							break;
						}
					} else {
						velocity = Vec3::zero;
						break;
					}
				} else {
					velocity = clipped;
				}
			}

			if (velocity.SqrLength() <= kEpsilon) {
				velocity = Vec3::zero;
				break;
			}
			if (velocity.Dot(primalVelocity) < 0.0f) {
				velocity = Vec3::zero;
				break;
			}
		}
	}

	void SphereKinematicCollisionResolver::StepMove(
		Vec3& position, Vec3& velocity, const float stepHeightHu,
		const float timeTotal
	) const {
		const float stepHeightM = std::max(0.0f, Math::HtoM(stepHeightHu));
		if (stepHeightM <= kEpsilon) {
			SlideMove(position, velocity, timeTotal);
			return;
		}

		auto HorizDistSq = [](const Vec3& a, const Vec3& b) -> float {
			const float dx = a.x - b.x;
			const float dz = a.z - b.z;
			return dx * dx + dz * dz;
		};

		const auto SnapDownWalkable = [this](Vec3& targetPos, const float maxDrop) {
			const float radius = HullRadiusM(mHull);
			if (maxDrop <= 0.0f || radius <= kEpsilon) {
				return false;
			}

			const float  skin       = SkinM();
			const float  castLength = maxDrop + skin;
			Physics::Hit hit{};
			if (!mEngine->SphereCast(targetPos, radius, Vec3::down, castLength, &hit)) {
				return false;
			}

			const bool walkable = hit.startSolid || hit.allsolid ||
			                      hit.normal.y > kGroundNormalY;
			if (!walkable) {
				return false;
			}

			const float hitDistance = HitDistanceM(hit, castLength);
			const float drop = std::clamp(hitDistance - skin, 0.0f, maxDrop);
			targetPos += Vec3::down * drop;
			return true;
		};

		// 1) 通常の SlideMove を先に実行して、基準となる移動結果を求めます。
		const Vec3 savedPos = position;
		const Vec3 savedVel = velocity;

		const Vec3  desiredMove        = savedVel * timeTotal;
		const float desiredHorizDistSq =
			desiredMove.x * desiredMove.x + desiredMove.z * desiredMove.z;

		SlideMove(position, velocity, timeTotal);
		Vec3 downPos = position;
		Vec3 downVel = velocity;
		if (SnapDownWalkable(downPos, stepHeightM) && downVel.y < 0.0f) {
			downVel.y = 0.0f;
		}

		const float downDistSq = HorizDistSq(downPos, savedPos);
		if (downDistSq + kStepSelectHorizEps >= desiredHorizDistSq) {
			position = downPos;
			velocity = downVel;
			return;
		}

		// 2) ステップアップ候補を試し、より前進できる場合のみ採用します。
		position = savedPos;
		velocity = savedVel;

		const float radius = HullRadiusM(mHull);
		float       stepRise = stepHeightM;
		{
			Physics::Hit upHit{};
			const float  skin         = SkinM();
			const float  upCastLength = stepHeightM + skin;

			if (mEngine->SphereCast(position, radius, Vec3::up, upCastLength, &upHit)) {
				const float hitDistance = HitDistanceM(upHit, upCastLength);
				stepRise = std::clamp(hitDistance - skin, 0.0f, stepHeightM);

				if (stepRise < skin) {
					position = downPos;
					velocity = downVel;
					return;
				}
			}

			position += Vec3::up * stepRise;
		}

		SlideMove(position, velocity, timeTotal);

		// 上げた分 + 許容ステップダウン分だけ地面へスナップを試みます。
		const float maxDrop    = stepRise + stepHeightM;
		const bool  upGrounded = SnapDownWalkable(position, maxDrop);
		if (!upGrounded) {
			position = downPos;
			velocity = downVel;
			return;
		}

		const float upDistSq       = HorizDistSq(position, savedPos);
		const bool  climbedTooHigh = position.y >
		                            (savedPos.y + stepHeightM +
		                             kStepSelectDownEps);

		if (climbedTooHigh || upDistSq <= downDistSq + kStepSelectHorizEps) {
			position = downPos;
			velocity = downVel;
			return;
		}

		velocity.y = std::max(velocity.y, 0.0f);
	}

	bool SphereKinematicCollisionResolver::ProbeGround(
		const Vec3& position, const float maxDistance, Physics::Hit* outHit
	) const {
		const float radius = HullRadiusM(mHull);
		if (!mEngine || maxDistance <= 0.0f || radius <= kEpsilon) {
			return false;
		}

		Physics::Hit hit{};
		const float  castDistance = maxDistance + SkinM();
		if (!mEngine->SphereCast(position, radius, Vec3::down, castDistance, &hit)) {
			return false;
		}

		if (outHit) {
			*outHit = hit;
		}
		return true;
	}

	int SphereKinematicCollisionResolver::CollectOverlaps(
		const Vec3&   position,
		const Vec3&   halfExtents,
		Physics::Hit* outHits,
		const int     maxHits
	) const {
		if (!mEngine || !outHits || maxHits <= 0) {
			return 0;
		}

		const float queryRadius = std::max(
			{
				std::fabs(halfExtents.x),
				std::fabs(halfExtents.y),
				std::fabs(halfExtents.z)
			}
		);
		const float radius = queryRadius > kEpsilon ? queryRadius : HullRadiusM(mHull);
		if (radius <= kEpsilon) {
			return 0;
		}

		return mEngine->SphereOverlapRaw(position, radius, outHits, maxHits);
	}

	bool SphereKinematicCollisionResolver::IsHullOverlapping(
		const Vec3&   position,
		Physics::Hit* outHit
	) const {
		const float radius = HullRadiusM(mHull);
		if (!mEngine || radius <= kEpsilon) {
			return false;
		}

		if (outHit) {
			return mEngine->SphereOverlapRaw(position, radius, outHit, 1) > 0;
		}

		Physics::Hit hit{};
		return mEngine->SphereOverlapRaw(position, radius, &hit, 1) > 0;
	}

	bool SphereKinematicCollisionResolver::RecoverFromPenetration(
		Vec3& position,
		const Vec3& preferredDirection
	) const {
		const float radius = HullRadiusM(mHull);
		if (!mEngine || radius <= kEpsilon) {
			return false;
		}

		if (!IsHullOverlapping(position, nullptr)) {
			return true;
		}

		const float skinM = SkinM();
		const float maxPushPerIter = Math::HtoM(kDepenetrationMaxPushHu);

		Vec3 preferredDir = preferredDirection;
		if (preferredDir.SqrLength() > kNormalEpsilon) {
			preferredDir.Normalize();
		} else {
			preferredDir = Vec3::zero;
		}

		for (uint32_t iter = 0; iter < kMaxDepenetrationIters; ++iter) {
			std::array<Physics::Hit, kDepenetrationHitBuffer> overlaps{};
			const int overlapCount = CollectOverlaps(
				position, Vec3(radius), overlaps.data(), kDepenetrationHitBuffer
			);
			if (overlapCount <= 0) {
				return true;
			}

			Vec3  pushAccum      = Vec3::zero;
			Vec3  deepestNormal  = Vec3::zero;
			float deepestPushLen = 0.0f;
			for (int i = 0; i < overlapCount; ++i) {
				Vec3 normal = overlaps[i].normal;
				if (normal.SqrLength() <= kNormalEpsilon) {
					continue;
				}
				normal.Normalize();

				const float pushLen = overlaps[i].depth + skinM;
				if (pushLen <= kDepenetrationDepthEpsilon) {
					continue;
				}

				pushAccum += normal * pushLen;
				if (pushLen > deepestPushLen) {
					deepestPushLen = pushLen;
					deepestNormal  = normal;
				}
			}

			if (deepestPushLen <= kDepenetrationDepthEpsilon) {
				break;
			}

			Vec3 pushDir = pushAccum.SqrLength() > kNormalEpsilon ?
					       pushAccum.Normalized() :
				       deepestNormal;
			if (
				!preferredDir.IsZero() &&
				pushDir.Dot(preferredDir) < 0.0f
			) {
				pushDir += preferredDir * 0.5f;
				if (pushDir.SqrLength() > kNormalEpsilon) {
					pushDir.Normalize();
				}
			}

			const float pushDist = std::clamp(
				deepestPushLen, skinM, maxPushPerIter
			);
			position += pushDir * pushDist;
			if (!IsHullOverlapping(position, nullptr)) {
				return true;
			}
		}

		const Vec3 searchOrigin = position;
		const std::array<Vec3, 16> probeDirs = {
			Vec3::right,
			Vec3::left,
			Vec3::forward,
			Vec3::backward,
			Vec3::up,
			Vec3::down,
			(Vec3::right + Vec3::forward).Normalized(),
			(Vec3::right + Vec3::backward).Normalized(),
			(Vec3::left + Vec3::forward).Normalized(),
			(Vec3::left + Vec3::backward).Normalized(),
			(Vec3::right + Vec3::up).Normalized(),
			(Vec3::left + Vec3::up).Normalized(),
			(Vec3::forward + Vec3::up).Normalized(),
			(Vec3::backward + Vec3::up).Normalized(),
			(Vec3::right + Vec3::down).Normalized(),
			(Vec3::left + Vec3::down).Normalized(),
		};
		const std::array<float, 8> probeRadiiHu = {
			0.25f,
			0.5f,
			1.0f,
			2.0f,
			4.0f,
			8.0f,
			16.0f,
			24.0f
		};

		for (const float radiusHu : probeRadiiHu) {
			const float radiusM = Math::HtoM(radiusHu);

			if (!preferredDir.IsZero()) {
				const Vec3 preferredCandidate = searchOrigin + preferredDir * radiusM;
				if (!IsHullOverlapping(preferredCandidate, nullptr)) {
					position = preferredCandidate;
					return true;
				}
			}

			for (const Vec3& dir : probeDirs) {
				const Vec3 candidate = searchOrigin + dir * radiusM;
				if (!IsHullOverlapping(candidate, nullptr)) {
					position = candidate;
					return true;
				}
			}
		}

		return !IsHullOverlapping(position, nullptr);
	}

	float SphereKinematicCollisionResolver::SkinM() {
		return Math::HtoM(kSkinHu);
	}
}

