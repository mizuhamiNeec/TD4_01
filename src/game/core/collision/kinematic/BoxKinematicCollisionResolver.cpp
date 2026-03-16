#include "BoxKinematicCollisionResolver.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "core/math/Math.h"
#include "engine/physics/core/Physics.h"

namespace Unnamed {
	namespace {
		constexpr float    kEpsilon            = 1e-6f;
		constexpr float    kNormalEpsilon      = 1e-12f;
		constexpr float    kDuplicatePlaneDot  = 0.99f;
		constexpr float    kSkinHu             = 1.0f; // 衝突面からの停止距離（HU）
		constexpr float    kOverbounce         = 1.0f;
		constexpr uint32_t kMaxClipPlanes      = 5;
		constexpr uint32_t kMaxSlideBump       = 8;
		constexpr float    kMinConsumedFrac    = 1e-5f;
		constexpr float    kZeroToiEpsilon     = 1e-6f;
		constexpr float    kZeroToiConsumed    = 0.05f;
		constexpr float    kTouchDepthRatio    = 0.01f;
		constexpr float    kGroundNormalY      = 0.7f; // ステップダウン判定の法線の最小Y成分
		constexpr float    kStepSelectHorizEps = 1e-8f;
		constexpr float    kStepSelectDownEps  = 1e-5f;
	}

	void BoxKinematicCollisionResolver::UpdateHull(
		const Vec3& pos, const Vec3& halfExtents
	) {
		mHull = {
			.center   = pos,
			.halfSize = halfExtents
		};
	}

	void BoxKinematicCollisionResolver::SlideMove(
		Vec3& position, Vec3& velocity, const float timeTotal
	) const {
		int   numPlanes = 0;
		float timeLeft  = timeTotal;

		const Vec3 primalVelocity = velocity;

		std::array<Vec3, kMaxClipPlanes> planes{};

		for (uint32_t bumpCount = 0; bumpCount < kMaxSlideBump; ++bumpCount) {
			if (velocity.SqrLength() < 1e-12f) {
				break;
			}
			const float preClipSpeed = velocity.Length();

			Vec3  move    = velocity * timeLeft;
			float moveLen = move.Length();
			if (moveLen < 1e-7f) {
				break;
			}

			Vec3 dir = move / moveLen;

			Box box    = mHull;
			box.center = position;
			Physics::Hit hit{};

			const float skin       = SkinM();
			const float castLength = moveLen + skin;

			if (!mEngine->BoxCast(
				box, dir, castLength, &hit
			)) {
				// 衝突なし — 全距離移動して終了
				position += move;
				break;
			}

			if (hit.allsolid) {
				velocity = Vec3::zero;
				return;
			}

			// ─── 前進距離を計算 ───
			// hit.t = [0, 1] の TOI。実距離に変換してから面から SkinM() 手前で止まる。moveLen を上限に。
			const float hitDistance =
				std::clamp(hit.t, 0.0f, 1.0f) * castLength;
			float allowedDist = std::min(
				moveLen,
				std::max(0.0f, hitDistance - skin)
			);

			// 法線を正規化
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
			// ゼロTOIかつ離反方向ならヒットを無視して移動を許可する。
			if (zeroToiContact && dir.Dot(normal) >= -kEpsilon) {
				position += move;
				break;
			}

			if (allowedDist > 1e-7f) {
				position += dir * allowedDist;
				// 前進に成功したら面リストをリセット（新しい位置で再出発）
				numPlanes = 0;
			}

			// 残り時間は実移動量基準で消費し、停滞回避のため最小消費のみ保証する。
			float consumedFrac = std::clamp(allowedDist / moveLen, 0.0f, 1.0f);
			consumedFrac       = std::max(
				consumedFrac,
				zeroToiContact ? kZeroToiConsumed : kMinConsumedFrac
			);
			timeLeft *= (1.0f - consumedFrac);
			if (timeLeft < 1e-7f) {
				break;
			}

			// ─── 同一面チェック ───
			bool duplicatePlane = false;
			for (int i = 0; i < numPlanes; ++i) {
				if (planes[i].Dot(normal) > kDuplicatePlaneDot) {
					// 同じ面にもう一度当たった。速度を面からクリップしてやり直す。
					velocity = ClipVelocity(velocity, normal, kOverbounce);
					const float clippedSpeed = velocity.Length();
					if (clippedSpeed > preClipSpeed + kEpsilon && clippedSpeed >
					    kEpsilon) {
						velocity *= (preClipSpeed / clippedSpeed);
					}
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

			// ─── クリッピング ───
			if (numPlanes == 1) {
				// 面が1枚 — シンプルにクリップ
				velocity = ClipVelocity(velocity, planes[0], kOverbounce);
			} else {
				// 複数面: 現在のvelocityをクリップし、全面と整合する解を探す
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
						// 2面の交線に沿って滑る
						Vec3  creaseDir = planes[0].Cross(planes[1]);
						float cLen      = creaseDir.Length();
						if (cLen > 1e-7f) {
							creaseDir = creaseDir / cLen;
							velocity  = creaseDir * velocity.Dot(creaseDir);
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
			const float clippedSpeed = velocity.Length();
			if (clippedSpeed > preClipSpeed + kEpsilon && clippedSpeed >
			    kEpsilon) {
				velocity *= (preClipSpeed / clippedSpeed);
			}

			// dead-stop: クリップ後の速度が元の方向と逆転したら停止
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

	void BoxKinematicCollisionResolver::StepMove(
		Vec3& position, Vec3& velocity, float stepHeight, float timeTotal
	) const {
		const float stepHeightM = std::max(0.0f, Math::HtoM(stepHeight));
		if (stepHeightM <= kEpsilon) {
			SlideMove(position, velocity, timeTotal);
			return;
		}

		auto horizDistSq = [](const Vec3& a, const Vec3& b) -> float {
			float dx = a.x - b.x;
			float dz = a.z - b.z;
			return dx * dx + dz * dz;
		};

		const auto snapDownWalkable = [this](Vec3& targetPos, const float maxDrop) -> bool {
			if (maxDrop <= 0.0f) {
				return false;
			}

			Box box = mHull;
			box.center = targetPos;

			const float  skin       = SkinM();
			const float  castLength = maxDrop + skin;
			Physics::Hit hit{};
			if (!mEngine->BoxCast(box, Vec3::down, castLength, &hit)) {
				return false;
			}

			const bool walkable = hit.startSolid || hit.allsolid ||
			                      hit.normal.y > kGroundNormalY;
			if (!walkable) {
				return false;
			}

			const float hitDistance = std::clamp(hit.t, 0.0f, 1.0f) * castLength;
			const float drop = std::clamp(hitDistance - skin, 0.0f, maxDrop);
			targetPos += Vec3::down * drop;
			return true;
		};

		// 1) 通常のSlideMoveを先に実行（baseline）
		const Vec3 savedPos = position;
		const Vec3 savedVel = velocity;

		const Vec3 desiredMove        = savedVel * timeTotal;
		const float desiredHorizDistSq =
			desiredMove.x * desiredMove.x + desiredMove.z * desiredMove.z;

		SlideMove(position, velocity, timeTotal);
		Vec3 downPos = position;
		Vec3 downVel = velocity;
		if (snapDownWalkable(downPos, stepHeightM) && downVel.y < 0.0f) {
			downVel.y = 0.0f;
		}

		const float downDistSq = horizDistSq(downPos, savedPos);
		// baselineで十分進めているときは、不要なステップアップを試さない。
		if (downDistSq + kStepSelectHorizEps >= desiredHorizDistSq) {
			position = downPos;
			velocity = downVel;
			return;
		}

		// 2) ステップ移動候補
		position = savedPos;
		velocity = savedVel;

		float stepRise = stepHeightM;
		{
			Box          boxUp = mHull;
			Physics::Hit upHit{};
			const float  skin         = SkinM();
			const float  upCastLength = stepHeightM + skin;
			boxUp.center              = position;

			if (mEngine->BoxCast(boxUp, Vec3::up, upCastLength, &upHit)) {
				const float hitDistance = std::clamp(upHit.t, 0.0f, 1.0f) *
				                          upCastLength;
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

		// 上げた分 + 許容ステップダウン分だけ地面へスナップを試みる。
		const float maxDrop = stepRise + stepHeightM;
		const bool  upGrounded = snapDownWalkable(position, maxDrop);
		if (!upGrounded) {
			position = downPos;
			velocity = downVel;
			return;
		}

		const float upDistSq       = horizDistSq(position, savedPos);
		const bool  climbedTooHigh = position.y >
		                             (savedPos.y + stepHeightM + kStepSelectDownEps);

		if (climbedTooHigh || upDistSq <= downDistSq + kStepSelectHorizEps) {
			position = downPos;
			velocity = downVel;
			return;
		}

		if (velocity.y < 0.0f) {
			velocity.y = 0.0f;
		}
	}

	bool BoxKinematicCollisionResolver::ProbeGround(
		const Vec3& position, const float maxDistance, Physics::Hit* outHit
	) const {
		if (!mEngine || maxDistance <= 0.0f) {
			return false;
		}

		Box box    = mHull;
		box.center = position;

		Physics::Hit hit{};
		const float  castDistance = maxDistance + CastSkinM();
		if (!mEngine->BoxCast(box, Vec3::down, castDistance, &hit)) {
			return false;
		}

		if (outHit) {
			*outHit = hit;
		}
		return true;
	}

	float BoxKinematicCollisionResolver::CastSkinM() {
		return SkinM();
	}

	float BoxKinematicCollisionResolver::SkinM() {
		return Math::HtoM(kSkinHu);
	}
}
