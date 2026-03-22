#pragma once

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

#include <engine/unnamed/physics/BVH.h>
#include <engine/unnamed/physics/BVHBuilder.h>
#include <engine/unnamed/physics/CollisionDetection.h>

#include "core/assets/types/MeshAssetData.h"

namespace Unnamed::Physics {
	/// @brief 物理エンジン
	class Engine {
	public:
		void Init();
		void Update(float deltaTime) const;
		void EndFrame();

		/// @brief レイキャストを行う関数
		/// @param ray レイ情報
		/// @param outHit 衝突情報の出力先
		/// @return 衝突した場合trueを返す
		bool RayCast(
			const Ray& ray,
			Hit*       outHit
		) const;

		/// @brief ボックスキャストを行う関数
		/// @param box ボックス情報
		/// @param dir キャスト方向
		/// @param length キャスト距離
		/// @param outHit 衝突情報の出力先
		/// @return 衝突した場合trueを返す
		bool BoxCast(
			const Box&  box,
			const Vec3& dir,
			float       length, Hit* outHit
		) const;

		/// @brief スフィアキャストを行う関数
		/// @param start スフィアの開始位置
		/// @param radius スフィアの半径
		/// @param dir キャスト方向
		/// @param length キャスト距離
		/// @param outHit 衝突情報の出力先
		/// @return 衝突した場合trueを返す
		bool SphereCast(
			const Vec3& start,
			float       radius,
			const Vec3& dir,
			float       length,
			Hit*        outHit
		) const;

		/// @brief ボックスとメッシュの重なり判定を行う関数
		/// @param box ボックス情報
		/// @param outHit 衝突情報の出力先
		/// @return 重なりがあった場合trueを返す
		bool BoxOverlap(
			const Box& box,
			Hit*       outHit
		) const;

		/// @brief ボックスとメッシュの重なり判定を行う関数（複数ヒット版）
		/// @param box ボックス情報
		/// @param outHits 衝突情報の出力先配列
		/// @param maxHits 出力先配列の最大ヒット数
		/// @return ヒットした数を返す
		int BoxOverlap(
			const Box& box,
			Hit*       outHits,
			int        maxHits
		) const;

		void ClearStaticMeshes();
		bool RegisterStaticMesh(
			uint64_t                    ownerGuid,
			std::span<const MeshVertex> vertices,
			std::span<const uint32_t>   indices,
			const Mat4&                 world
		);
		bool RegisterDynamicMesh(
			uint64_t                    ownerGuid,
			std::span<const MeshVertex> vertices,
			std::span<const uint32_t>   indices,
			const Mat4&                 world
		);
		bool UpdateDynamicMeshTransform(
			uint64_t    ownerGuid,
			const Mat4& world
		);
		void UnregisterStaticMesh(uint64_t ownerGuid);
		void UnregisterDynamicMesh(uint64_t ownerGuid);

		[[nodiscard]] std::vector<Box> GetDebugBVHBoxes() const;

	private:
		[[nodiscard]] static AABB ToWorldBounds(
			const RegisteredBVH& bvh,
			const AABB&          localBounds
		) {
			if (bvh.mobility == ColliderMobility::Dynamic) {
				const Vec3 corners[8]{
					{localBounds.min.x, localBounds.min.y, localBounds.min.z},
					{localBounds.max.x, localBounds.min.y, localBounds.min.z},
					{localBounds.min.x, localBounds.max.y, localBounds.min.z},
					{localBounds.max.x, localBounds.max.y, localBounds.min.z},
					{localBounds.min.x, localBounds.min.y, localBounds.max.z},
					{localBounds.max.x, localBounds.min.y, localBounds.max.z},
					{localBounds.min.x, localBounds.max.y, localBounds.max.z},
					{localBounds.max.x, localBounds.max.y, localBounds.max.z},
				};
				AABB worldBounds;
				for (const Vec3& p : corners) {
					worldBounds.Expand(bvh.world.TransformPoint(p));
				}
				return worldBounds;
			}
			return localBounds;
		}

		[[nodiscard]] static Triangle ToWorldTriangle(
			const RegisteredBVH& bvh,
			uint32_t             triIdx
		) {
			const Triangle& tri = bvh.triangles[triIdx];
			if (bvh.mobility == ColliderMobility::Dynamic) {
				return {
					.v0 = bvh.world.TransformPoint(tri.v0),
					.v1 = bvh.world.TransformPoint(tri.v1),
					.v2 = bvh.world.TransformPoint(tri.v2),
				};
			}
			return tri;
		}

		template <class CastType>
		static bool CastBVH(
			const CastType&                   cast,
			const Vec3&                       start,
			const Vec3&                       dir,
			float                             length,
			Hit*                              outHit,
			const std::vector<RegisteredBVH>& bvhSet,
			std::vector<Box>*                 debugBox
		) {
			std::vector<const RegisteredBVH*> filtered;
			const Ray broadRay = {
				.origin = start,
				.dir    = dir,
				.invDir = Vec3::one / dir,
				.tMin   = 0.0f,
				.tMax   = length
			};
			Vec3        dirNormalized = dir;
			const float dirLenSq      = dirNormalized.SqrLength();
			if (dirLenSq > 1e-12f) {
				dirNormalized /= std::sqrt(dirLenSq);
			} else {
				dirNormalized = Vec3::zero;
			}

			for (const auto& bvh : bvhSet) {
				if (bvh.nodes.empty()) {
					continue;
				}
				AABB  root = cast.ExpandNode(ToWorldBounds(bvh, bvh.nodes[0].bounds));
				float t    = length;
				if (RayVsAABB(broadRay, root, t)) {
					filtered.emplace_back(&bvh);
				}
			}

			if (filtered.empty()) {
				return false;
			}

			constexpr float kStartSolidToiEpsilon = 1e-6f;
			constexpr float kNormalEpsilon        = 1e-12f;
			constexpr float kOverlapDepthEpsilon  = 1e-6f;

			float    bestTOI       = 1.0f;
			uint32_t hitTri        = UINT32_MAX;
			uint64_t hitEntityGuid = 0;
			Vec3     hitNormal;
			Triangle hitTriangle{};
			bool     hasHitTriangle = false;

#ifdef _DEBUG
			struct StackItem {
				uint32_t nodeIndex;
				int      parentEntry;
			};
			struct PathEntry {
				uint32_t nodeIndex;
				int      parentEntry;
			};
			const RegisteredBVH* bestPathBVH   = nullptr;
			std::vector<PathEntry> bestPathData;
			int                    bestPathLeaf = -1;
#else
			struct StackItem {
				uint32_t nodeIndex;
			};
#endif
			StackItem stack[64];

			for (auto* bvh : filtered) {
#ifdef _DEBUG
				std::vector<PathEntry> traversal;
#endif
				int sp      = 0;
				stack[sp++] = StackItem{
					.nodeIndex = 0
#ifdef _DEBUG
					,
					.parentEntry = -1
#endif
				};

				while (sp) {
#ifdef _DEBUG
					const StackItem item = stack[--sp];
					const uint32_t index = item.nodeIndex;
					const int currentEntry = static_cast<int>(traversal.size());
					traversal.emplace_back(
						PathEntry{.nodeIndex = index, .parentEntry = item.parentEntry}
					);
#else
					const uint32_t index = stack[--sp].nodeIndex;
#endif
					const auto& node = bvh->nodes[index];

					Ray pruneRay  = broadRay;
					pruneRay.tMax = bestTOI * length;
					float tBox    = bestTOI * length;
					const AABB nodeBoundsWorld = ToWorldBounds(*bvh, node.bounds);
					if (!RayVsAABB(pruneRay, cast.ExpandNode(nodeBoundsWorld), tBox)) {
						continue;
					}

					if (node.primCount == 0) {
						stack[sp++] = StackItem{
							.nodeIndex = node.leftFirst
#ifdef _DEBUG
							,
							.parentEntry = currentEntry
#endif
						};
						stack[sp++] = StackItem{
							.nodeIndex = node.rightFirst
#ifdef _DEBUG
							,
							.parentEntry = currentEntry
#endif
						};
					} else {
						const uint32_t first = node.leftFirst;
						for (uint32_t i = 0; i < node.primCount; ++i) {
							const uint32_t triIdx = bvh->triIndices[first + i];
							const Triangle tri    = ToWorldTriangle(*bvh, triIdx);
							float          toi;
							Vec3           nrm;
							if (!cast.TestTriangle(tri, dir, length, toi, nrm)) {
								continue;
							}
							if (toi < bestTOI) {
								bestTOI         = toi;
								hitTri          = triIdx;
								hitEntityGuid   = bvh->ownerGuid;
								hitNormal       = nrm;
								hitTriangle     = tri;
								hasHitTriangle  = true;
#ifdef _DEBUG
								bestPathBVH     = bvh;
								bestPathData    = traversal;
								bestPathLeaf    = currentEntry;
#endif
								pruneRay.tMax = bestTOI * length;
							}
						}
					}
				}
			}

			if (hitTri == UINT32_MAX || !hasHitTriangle) {
				return false;
			}

#ifdef _DEBUG
			if (debugBox != nullptr) {
				debugBox->clear();
				if (bestPathBVH && bestPathLeaf >= 0) {
					std::vector<uint32_t> pathNodes;
					for (int cursor = bestPathLeaf; cursor >= 0;) {
						const PathEntry& entry = bestPathData[cursor];
						pathNodes.emplace_back(entry.nodeIndex);
						cursor = entry.parentEntry;
					}
					std::reverse(pathNodes.begin(), pathNodes.end());

					for (const uint32_t nodeIndex : pathNodes) {
						const AABB bounds = ToWorldBounds(
							*bestPathBVH,
							bestPathBVH->nodes[nodeIndex].bounds
						);
						const Vec3 center = (bounds.min + bounds.max) * 0.5f;
						const Vec3 size   = bounds.max - bounds.min;
						debugBox->emplace_back(center, size);
					}
				}
			}
#endif

			if (outHit) {
				float overlapDepth = 0.0f;
				Vec3  overlapNrm   = Vec3::zero;
				bool  startSolid   = false;
				if (
					bestTOI <= kStartSolidToiEpsilon ||
					hitNormal.SqrLength() <= kNormalEpsilon
				) {
					startSolid = cast.OverlapAtStart(
						hitTriangle,
						overlapDepth,
						overlapNrm
					);
					if (startSolid && overlapDepth <= kOverlapDepthEpsilon) {
						startSolid   = false;
						overlapDepth = 0.0f;
					}
					if (
						startSolid &&
						overlapNrm.SqrLength() > kNormalEpsilon
					) {
						hitNormal = overlapNrm;
					}
				}

				Vec3        finalNormal = hitNormal;
				const float nLenSq      = finalNormal.SqrLength();
				if (nLenSq > 1e-12f) {
					finalNormal /= std::sqrt(nLenSq);
				} else {
					finalNormal = (hitTriangle.v1 - hitTriangle.v0).Cross(
						hitTriangle.v2 - hitTriangle.v0
					);
					const float triNLenSq = finalNormal.SqrLength();
					if (triNLenSq > kNormalEpsilon) {
						finalNormal /= std::sqrt(triNLenSq);
						if (finalNormal.Dot(dirNormalized) > 0.0f) {
							finalNormal = -finalNormal;
						}
					} else if (dirLenSq > kNormalEpsilon) {
						finalNormal = -dirNormalized;
					} else {
						finalNormal = Vec3::up;
					}
				}
				outHit->t      = bestTOI;
				outHit->depth  = startSolid ? overlapDepth : 0.0f;
				outHit->normal = finalNormal;
				outHit->pos    = cast.ComputeImpactPoint(
					start,
					dirNormalized,
					length,
					bestTOI,
					finalNormal
				);
				outHit->triIndex      = hitTri;
				outHit->hitEntityGuid = hitEntityGuid;
				outHit->startSolid    = startSolid;
				outHit->allsolid      = false;
			}
			return true;
		}

		static bool RemoveColliderByOwnerGuid(
			std::vector<RegisteredBVH>& bvhSet,
			uint64_t                   ownerGuid
		);
		static RegisteredBVH BuildRegisteredMesh(
			uint64_t                    ownerGuid,
			std::span<const MeshVertex> vertices,
			std::span<const uint32_t>   indices,
			const Mat4&                 world,
			ColliderMobility            mobility
		);

		static int BoxOverlapSet(
			const Box&                        box,
			Hit*                              outHits,
			int                               maxHits,
			const std::vector<RegisteredBVH>& bvhSet
		);

		std::vector<RegisteredBVH> mStaticBVHs;
		std::vector<RegisteredBVH> mDynamicBVHs;

		mutable std::vector<Box> mDebugBox;
	};
}
