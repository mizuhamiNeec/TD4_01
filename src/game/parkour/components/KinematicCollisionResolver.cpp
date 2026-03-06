#include "KinematicCollisionResolver.h"

#include "engine/unnamed/framework/components/TransformComponent.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	// static constexpr std::string_view kChannel = "KCR";
	//
	// constexpr float kEpsilon = 1e-6f;
	//
	// constexpr float kStuckThreshold     = 0.1f; // スタックとみなす移動距離の閾値 (m/s)
	// constexpr float kStuckTimeThreshold = 0.5f; // スタック状態になるまでの時間 (秒)
	//
	// constexpr uint32_t kMaxDepenetrationCount = 4;  // 貫通解消の最大反復回数
	// constexpr uint32_t kMaxDepenetrationHits  = 32; // BoxOverlapで考慮する最大ヒット数
	//
	// constexpr size_t kMaxClipPlanes = 5; // Sourceも5
	// constexpr int    kMaxBumps      = 8;
	//
	// constexpr float kStepHeightHu  = 18.0f;
	// constexpr float kCastSkinHu    = 0.5f;
	// constexpr float kSkinHu        = 1.0f; // 衝突面からの停止距離
	// constexpr float kRestOffsetHu  = 0.75f;
	// constexpr float kMaxAdhesionHu = 2.0f; // 接地
	//
	//
	// constexpr float kOverbounce = 1.0f;
	// // 1.0f で完全に反発、1.01f なら少し余計に反発して滑りやすくなる TODO: 調整の余地あり
	//
	// KinematicCollisionResolver::KinematicCollisionResolver(
	// 	UPhysics::Engine*   physicsEngine, MovementData* data,
	// 	TransformComponent* transform, Box*              hull
	// ) : mPhysicsEngine(physicsEngine),
	//     mData(data),
	//     mTransform(transform),
	//     mHull(hull) { UpdateEntityHullDimensions(); }
	//
	// void KinematicCollisionResolver::MoveWithCollisions(float dt) {
	// 	// もう疲れちゃって 全然動けなくてェ…
	// 	if (!mPhysicsEngine) {
	// 		// 移動だけする
	// 		mTransform->SetPosition(
	// 			mTransform->Position() + mData->velocity * dt
	// 		);
	// 		mData->isGrounded = false; // そもそも何にも触れられん
	// 		UpdateEntityHullDimensions();
	// 		return;
	// 	}
	//
	// 	// めり込んでいる場合は解消
	// 	ResolvePenetration();
	//
	// 	Vec3 pos = mTransform->Position();
	// 	Vec3 vel = mData->velocity;
	//
	// 	const float horizontalVelSqr = Vec3(vel.x, 0.0f, vel.z).SqrLength();
	// 	const bool  wantStep         = mData->wasGroundedLastFrame &&
	// 	                               (horizontalVelSqr > kEpsilon);
	//
	// 	if (wantStep) {
	// 		// ステップ移動
	// 		StepMove(pos, vel, dt);
	// 	} else {
	// 		// スライド移動
	// 		SlideMove(pos, vel, dt);
	// 	}
	//
	// 	// 接地しているか確認
	// 	bool isGrounded = GroundCheck(pos);
	//
	// 	mTransform->SetPosition(pos);
	// 	mData->velocity   = vel;
	// 	mData->isGrounded = isGrounded;
	//
	// 	UpdateEntityHullDimensions();
	//
	// 	// 移動後にめり込んでいる場合は解消
	// 	ResolvePenetration();
	//
	// 	// 接地している場合、下向き速度をゼロに
	// 	if (mData->isGrounded && mData->velocity.y < 0.0f) {
	// 		mData->velocity.y = 0.0f;
	// 	}
	//
	// 	// 壁走りやスライド中でなければ、地上か空中かで状態を切り替える
	// 	if (!mData->isWallRunning && !mData->isSliding) {
	// 		mData->state = mData->isGrounded ?
	// 			               MOVEMENT_STATE::GROUND :
	// 			               MOVEMENT_STATE::AIR;
	// 	}
	//
	// 	UpdateEntityHullDimensions();
	// }
	//
	// bool KinematicCollisionResolver::CollideAndSlide(
	// 	const Vec3& startPos, const Vec3& displacement, Vec3& outPos
	// ) {
	// 	outPos = startPos + displacement; // まずは理想的な位置を計算
	// 	// CollideもSlideもない!
	// 	if (!mPhysicsEngine) { return !displacement.IsZero(); }
	//
	// 	Vec3 pos = startPos;
	// 	Vec3 vel = displacement;
	// 	SlideMove(pos, vel, 1.0f);
	// 	outPos = pos;
	// 	return (outPos - startPos).SqrLength() > kEpsilon;
	// }
	//
	// void KinematicCollisionResolver::SyncHullFromComponent() {
	// 	UpdateEntityHullDimensions(); // Transformの変更に合わせてハルの位置も更新
	// }
	//
	// void KinematicCollisionResolver::DetectAndResolveStuck(float dt) {
	// 	if (!mPhysicsEngine) { return; } // 物理エンジンが無い場合はスタック検出もできない
	//
	// 	Vec3  currentPos = mTransform->Position();
	// 	float distMoved  = (currentPos - mData->lastPosition).Length();
	//
	// 	// プレイヤーが暴れているのにほとんど移動していない場合、スタックかも
	// 	const bool hasInput = mData->vecMoveInput.x != 0.0f ||
	// 	                      mData->vecMoveInput.y != 0.0f ||
	// 	                      mData->wishJump;
	//
	// 	if (hasInput && distMoved < kStuckThreshold * dt) {
	// 		mData->stuckTime += dt;
	// 		if (mData->stuckTime >= kStuckTimeThreshold) {
	// 			mData->isStuck = true;
	//
	// 			// いろんな方向に少しずつ移動してみて、どこかに抜けられないか試す
	// 			Vec3 escapeAttempts[] = {
	// 				Vec3::up,    // まずは真上に
	// 				Vec3::right, // 水平4方向も試す
	// 				Vec3::left,
	// 				Vec3::forward,
	// 				Vec3::backward,
	// 				Vec3::up + Vec3::right, // 斜めも試す
	// 				Vec3::up + Vec3::left,
	// 				Vec3::up + Vec3::forward,
	// 				Vec3::up + Vec3::backward,
	// 				Vec3::forward + Vec3::right, // 水平斜めも試す
	// 				Vec3::forward + Vec3::left,
	// 				Vec3::backward + Vec3::right,
	// 				Vec3::backward + Vec3::left,
	// 				Vec3::down,               // 最終手段
	// 				Vec3::down + Vec3::right, // 下斜めも試す
	// 				Vec3::down + Vec3::left,
	// 				Vec3::down + Vec3::forward,
	// 				Vec3::down + Vec3::backward,
	// 			}; // 正規化してない
	//
	// 			bool escaped = false;
	// 			for (const Vec3& escapeVel : escapeAttempts) {
	// 				Vec3 testPos =
	// 					currentPos + escapeVel.Normalized() * dt * 2.0f;
	// 				mTransform->SetPosition(testPos);
	// 				UpdateEntityHullDimensions();
	//
	// 				if (mPhysicsEngine) {
	// 					UPhysics::Hit ov = {};
	// 					if (!mPhysicsEngine->BoxOverlap(*mHull, &ov)) {
	// 						mData->velocity += escapeVel;
	// 						escaped         = true;
	// 						break;
	// 					}
	// 				}
	// 			}
	//
	// 			if (!escaped) {
	// 				mTransform->SetPosition(currentPos);
	// 				UpdateEntityHullDimensions();
	// 			}
	// 			mData->stuckTime = 0.0f; // 一応試したのでリセット
	// 		}
	// 	} else {
	// 		// スタックしてない、もしくは十分動けてる → スタック時間を減らす
	// 		mData->stuckTime = std::max(0.0f, mData->stuckTime - dt * 2.0f);
	// 		// スタック時間がゼロになったらスタック状態も解除
	// 		if (mData->stuckTime == 0.0f) { mData->isStuck = false; }
	// 	}
	// 	mData->lastPosition = mTransform->Position();
	// }
	//
	// Box KinematicCollisionResolver::BuildHullAtFeet(const Vec3& feetPos) const {
	// 	return Box{
	// 		.center = feetPos +
	// 		          Vec3::up *
	// 		          Math::HtoM(
	// 			          mData->currentHeightHu * 0.5f
	// 		          ),
	// 		.halfSize = Math::HtoM(
	// 			{
	// 				mData->currentWidthHu * 0.5f,
	// 				mData->currentHeightHu * 0.5f,
	// 				mData->currentWidthHu * 0.5f,
	// 			}
	// 		)
	// 	};
	// }
	//
	// void KinematicCollisionResolver::SetData(MovementData* movementData) {
	// 	mData = movementData;
	// 	UpdateEntityHullDimensions(); // データが変わったらハルの寸法も更新
	// }
	//
	// float KinematicCollisionResolver::StepHeightM() const {
	// 	return Math::HtoM(kStepHeightHu);
	// }
	//
	// float KinematicCollisionResolver::CastSkinM() const {
	// 	return Math::HtoM(kCastSkinHu);
	// }
	//
	// float KinematicCollisionResolver::SkinM() const {
	// 	return Math::HtoM(kSkinHu);
	// }
	//
	// float KinematicCollisionResolver::RestOffsetM() const {
	// 	return Math::HtoM(kRestOffsetHu);
	// }
	//
	// float KinematicCollisionResolver::MaxAdhesionM() const {
	// 	return Math::HtoM(kMaxAdhesionHu);
	// }
	//
	// void KinematicCollisionResolver::ResolvePenetration() {
	// 	// もうなにもできないわ...
	// 	if (!mPhysicsEngine) { return; }
	//
	// 	for (int i = 0; i < kMaxDepenetrationCount; ++i) {
	// 		// ハルの位置と寸法を最新にしてからチェック
	// 		UpdateEntityHullDimensions();
	//
	// 		UPhysics::Hit hits[kMaxDepenetrationHits];
	// 		int           hitCount = mPhysicsEngine->BoxOverlap(
	// 			*mHull, hits, kMaxDepenetrationHits
	// 		);
	// 		if (hitCount == 0) { break; } // 貫通なし → 解消完了
	//
	// 		Vec3 totalPush = Vec3::zero;
	// 		bool anyPush   = false;
	//
	// 		for (int j = 0; j < hitCount; ++j) {
	// 			const UPhysics::Hit& h = hits[j];
	// 			if (h.depth <= 0.0f) { continue; } // 貫通してないやつは無視
	//
	// 			Vec3 normal = h.normal;
	// 			if (normal.SqrLength() < 1e-12f) { continue; } // 法線が無効なら無視
	// 			normal.Normalize();
	//
	// 			float pushDistance = h.depth + SkinM(); // 貫通深度にマージンを加える
	//
	// 			// すでに同じ方向に十分押し出しているなら、このヒットは無視
	// 			float projected = totalPush.Dot(normal);
	// 			if (projected >= pushDistance) { continue; }
	//
	// 			// まだ足りない分だけ押し出す
	// 			float remaining = pushDistance - std::max(0.0f, projected);
	//
	// 			totalPush = totalPush + normal * remaining;
	// 			anyPush   = true;
	// 		}
	//
	// 		if (!anyPush) { break; } // どのヒットも十分押し出せている → 解消完了
	//
	// 		mTransform->SetPosition(mTransform->Position() + totalPush);
	// 		UpdateEntityHullDimensions();
	//
	// 		// 押し出し方向に対して速度が「めり込む方向」なら除去
	// 		Vec3  pushDir     = totalPush.Normalized();
	// 		float velIntoWall = mData->velocity.Dot(pushDir);
	// 		if (velIntoWall < 0.0f) {
	// 			mData->velocity -= pushDir * velIntoWall;
	// 		}
	// 	}
	// }
	//
	// int KinematicCollisionResolver::SlideMove(
	// 	Vec3& position, Vec3& velocity, float timeTotal
	// ) {
	// 	int   blocked   = 0;
	// 	int   numplanes = 0;
	// 	float timeLeft  = timeTotal;
	//
	// 	Vec3 primalVelocity = velocity;
	//
	// 	std::array<Vec3, kMaxClipPlanes> planes = {};
	//
	// 	for (int bumpCount = 0; bumpCount < kMaxBumps; ++bumpCount) {
	// 		// 速度がほとんどゼロなら停止
	// 		if (velocity.SqrLength() < 1e-12f) { break; }
	//
	// 		// 移動予定位置を計算
	// 		Vec3  move    = velocity * timeLeft;
	// 		float moveLen = move.Length();
	//
	// 		if (moveLen < 1e-7f) { break; } // 移動距離がほとんどゼロなら停止
	//
	// 		Vec3 dir = move / moveLen; // 移動方向 (正規化)
	//
	// 		Box           box = BuildHullAtFeet(position);
	// 		UPhysics::Hit hit{};
	//
	// 		if (
	// 			!mPhysicsEngine->BoxCast(
	// 				box,
	// 				dir,
	// 				moveLen + CastSkinM(), // キャスト距離にスキン分を加える
	// 				&hit
	// 			)
	// 		) {
	// 			// 衝突なし — 全距離移動して終了
	// 			position += move;
	// 			break;
	// 		}
	//
	// 		// allsolid — 完全に固体内
	// 		if (hit.allsolid) {
	// 			velocity = Vec3::zero;
	// 			return 4; // もうダメだ…おしまいだぁ…
	// 		}
	//
	// 		// 前進距離を計算
	// 		float allowed = std::min(moveLen, std::max(0.0f, hit.t - SkinM()));
	//
	// 		if (allowed > 1e-7f) {
	// 			position  += dir * allowed;
	// 			numplanes = 0; // 前進に成功したら面をリセット
	// 		}
	//
	// 		if (hit.startSolid && allowed <= 1e-7f) {
	// 			Vec3 pushNormal = hit.normal;
	// 			if (pushNormal.SqrLength() > 1e-12f) {
	// 				pushNormal.Normalize();
	// 				position += pushNormal * SkinM();
	// 			}
	// 		}
	//
	// 		// 残り時間を消費
	// 		float frac = (moveLen > 1e-7f) ? (allowed / moveLen) : 1.0f;
	// 		timeLeft   *= (1.0f - std::clamp(frac, 0.0f, 1.0f));
	// 		if (timeLeft < 1e-7f) { break; }
	//
	// 		// 法線を正規化
	// 		Vec3 normal = hit.normal;
	// 		if (normal.SqrLength() > 1e-12f) { normal.Normalize(); }
	//
	// 		// 同一面かチェック
	// 		bool duplicatePlane = false;
	// 		for (int i = 0; i < numplanes; ++i) {
	// 			if (planes[i].Dot(normal) > 0.99f) {
	// 				// 同じ面にまた当たった → 速度を面からクリップしてやり直す
	// 				velocity = ClipVelocity(velocity, normal, kOverbounce);
	// 				duplicatePlane = true;
	// 				break;
	// 			}
	// 		}
	//
	// 		if (duplicatePlane) { continue; } // 同一面ならこれ以上の処理は不要
	//
	// 		if (numplanes >= kMaxClipPlanes) {
	// 			// これ以上面が増えるのは想定してない → 完全に停止
	// 			velocity = Vec3::zero;
	// 			break;
	// 		}
	//
	// 		planes[numplanes++] = normal;
	//
	// 		// クリップ
	// 		if (numplanes == 1) {
	// 			// 面が1枚 — 普通にクリップ
	// 			velocity = ClipVelocity(velocity, planes[0], kOverbounce);
	// 		} else {
	// 			// 複数面 — クリップして、全部の面といい感じになる解を探す
	// 			Vec3 clipped;
	// 			int  i;
	// 			for (i = 0; i < numplanes; ++i) {
	// 				clipped = ClipVelocity(velocity, planes[i], kOverbounce);
	//
	// 				bool valid = true;
	// 				for (int j = 0; j < numplanes; ++j) {
	// 					if (j == i) { continue; } // 一回クリップした面は無視
	// 					if (clipped.Dot(planes[j]) < 0.0f) {
	// 						valid = false;
	// 						break;
	// 					}
	// 				}
	// 				if (valid) break; // 全面といい感じならこれを採用
	// 			}
	//
	// 			if (i == numplanes) {
	// 				if (numplanes == 2) {
	// 					// 2面の交線に沿って滑る
	// 					Vec3  creaseDir = planes[0].Cross(planes[1]);
	// 					float cLen      = creaseDir.Length();
	// 					if (cLen > 1e-7f) {
	// 						// 交線方向に速度を投影
	// 						creaseDir = creaseDir / cLen;
	// 						velocity  = creaseDir * velocity.Dot(creaseDir);
	// 					} else {
	// 						// 面がほぼ平行 → どこにも滑れない
	// 						velocity = Vec3::zero;
	// 						break;
	// 					}
	// 				} else {
	// 					// 3面以上 → どこにも滑れない
	// 					velocity = Vec3::zero;
	// 					break;
	// 				}
	// 			} else {
	// 				// いい感じの解が見つかった
	// 				velocity = clipped;
	// 			}
	// 		}
	//
	// 		// クリップした結果が反対方向の場合は止める
	// 		if (velocity.Dot(primalVelocity) < 0.0f) {
	// 			velocity = Vec3::zero;
	// 			break;
	// 		}
	// 	}
	//
	// 	return blocked;
	// }
	//
	// void KinematicCollisionResolver::StepMove(
	// 	Vec3& position, Vec3& velocity, float timeTotal
	// ) {
	// 	// スライドしてみる
	// 	const Vec3 savedPos = position;
	// 	const Vec3 savedVel = velocity;
	//
	// 	SlideMove(position, velocity, timeTotal);
	//
	// 	const Vec3 downPos = position;
	// 	const Vec3 downVel = velocity;
	//
	// 	// ステップ移動を試す
	// 	position = savedPos;
	// 	velocity = savedVel;
	//
	// 	{
	// 		Box           boxUp = BuildHullAtFeet(position);
	// 		UPhysics::Hit upHit{};
	// 		float         stepUp = StepHeightM();
	//
	// 		if (mPhysicsEngine->BoxCast(
	// 			boxUp, Vec3::up, stepUp + SkinM(), &upHit
	// 		)) {
	// 			// 天井に当たった場合、上がれる分だけ上げる
	// 			float allowed = std::max(0.0f, upHit.t - SkinM());
	// 			if (allowed < SkinM()) {
	// 				// ほとんど上がれない → スライド移動の結果を使う
	// 				position = downPos;
	// 				velocity = downVel;
	// 				return;
	// 			}
	// 			position += Vec3::up * allowed; // 可能な限り上げる
	// 		} else {
	// 			position += Vec3::up * stepUp; // 天井なし → ステップ分上げる
	// 		}
	// 	}
	//
	// 	// 上げた位置でスライド移動
	// 	SlideMove(position, velocity, timeTotal);
	//
	// 	// 下方向に降ろす
	// 	{
	// 		Box           boxDown = BuildHullAtFeet(position);
	// 		UPhysics::Hit downHit{};
	// 		float         stepDown = StepHeightM();
	//
	// 		if (
	// 			mPhysicsEngine->BoxCast(
	// 				boxDown,
	// 				Vec3::up,
	// 				stepDown,
	// 				&downHit
	// 			)
	// 		) {
	// 			// 何かに当たったら、歩行可能面かどうかに関わらず降ろす
	// 			const float drop = std::max(0.0f, downHit.t - SkinM());
	// 			position         += Vec3::down * drop;
	// 		}
	// 	}
	//
	// 	// TODO: この先必要か検証
	// }
	//
	// bool KinematicCollisionResolver::GroundCheck(Vec3& position) {
	// 	// ジャンプスナップ無効中や上昇中は地面にくっつかない
	// 	if (mData->jumpSnapDisableTime > 0.0f || mData->velocity.y > 0.0f) {
	// 		return false;
	// 	}
	//
	// 	Box           box       = BuildHullAtFeet(position);
	// 	UPhysics::Hit groundHit = {};
	//
	// 	float snapRange;
	// 	if (mData->wasGroundedLastFrame) {
	// 		snapRange = RestOffsetM() + std::max(MaxAdhesionM(), StepHeightM());
	// 	} else {
	// 		// 空中にいる場合は、ほんの少しだけでも地面にくっつくようにする
	// 		snapRange = RestOffsetM() + CastSkinM();
	// 	}
	//
	// 	if (!mPhysicsEngine->BoxCast(box, Vec3::down, snapRange, &groundHit)) {
	// 		return false;
	// 	}
	//
	// 	const float threshold = mData->groundNormalY;
	// 	if (groundHit.normal.y < threshold) {
	// 		return false; // 法線の傾きが地面とみなせる範囲外
	// 	}
	//
	// 	const float drop = std::max(0.0f, groundHit.t - RestOffsetM());
	// 	position         += Vec3::down * drop; // ほんの少しでも地面にめり込んでるなら降ろす 
	//
	// 	mData->lastGroundNormal = groundHit.normal; // 法線を保存しておく
	// 	mData->lastGroundDistM  = groundHit.t;      // 地面までの距離も保存しておく
	//
	// 	return true;
	// }
	//
	// Vec3 KinematicCollisionResolver::ClipVelocity(
	// 	const Vec3& vel, const Vec3& normal, float overbounce
	// ) {
	// 	const float backoff = vel.Dot(normal) * overbounce;
	// 	Vec3        out     = vel - normal * backoff;
	//
	// 	constexpr float kStopEps = 1e-6f;
	// 	if (std::fabs(out.x) < kStopEps) { out.x = 0.0f; }
	// 	if (std::fabs(out.y) < kStopEps) { out.y = 0.0f; }
	// 	if (std::fabs(out.z) < kStopEps) { out.z = 0.0f; }
	// 	return out;
	// }
	//
	// void KinematicCollisionResolver::UpdateEntityHullDimensions() {
	// 	// 足元原点
	// 	*mHull = {
	// 		.center =
	// 		mTransform->Position() +
	// 		Vec3::up * Math::HtoM(mData->currentHeightHu * 0.5f),
	// 		.halfSize = Math::HtoM(
	// 			{
	// 				mData->currentWidthHu * 0.5f,
	// 				mData->currentHeightHu * 0.5f,
	// 				mData->currentWidthHu * 0.5f,
	// 			}
	// 		)
	// 	};
	// }
}
