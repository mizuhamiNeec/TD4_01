#pragma once
#include <cmath>
#include <span>
#include <engine/unnamed/uphysics/BVH.h>
#include <engine/unnamed/uphysics/BVHBuilder.h>
#include <engine/unnamed/uphysics/CollisionDetection.h>

#include "core/assets/types/MeshAssetData.h"

namespace UPhysics {
	/// @brief 物理エンジン
	class Engine {
	public:
		void Init();
		void Update(float deltaTime) const;

		/// @brief レイキャストを行う関数
		/// @param ray レイ情報
		/// @param outHit 衝突情報の出力先
		/// @return 衝突した場合trueを返す
		bool RayCast(
			const Unnamed::Ray& ray,
			Hit*                outHit
		) const;

		/// @brief ボックスキャストを行う関数
		/// @param box ボックス情報
		/// @param dir キャスト方向
		/// @param length キャスト距離
		/// @param outHit 衝突情報の出力先
		/// @return 衝突した場合trueを返す
		bool BoxCast(
			const Unnamed::Box& box,
			const Vec3&         dir,
			float               length, Hit* outHit
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
			const Unnamed::Box& box,
			Hit*                outHit
		) const;

		/// @brief ボックスとメッシュの重なり判定を行う関数（複数ヒット版）
		/// @param box ボックス情報
		/// @param outHits 衝突情報の出力先配列
		/// @param maxHits 出力先配列の最大ヒット数
		/// @return ヒットした数を返す
		int BoxOverlap(
			const Unnamed::Box& box,
			Hit*                outHits,
			int                 maxHits
		) const;

		void ClearStaticMeshes();
		bool RegisterStaticMesh(
			uint64_t                             ownerGuid,
			std::span<const Unnamed::MeshVertex> vertices,
			std::span<const uint32_t>            indices,
			const Mat4&                          world
		);
		void UnregisterStaticMesh(uint64_t ownerGuid);

	private:
		template <class CastType>
		static bool CastBVH(
			const CastType&                       cast,
			const Vec3&                           start,
			const Vec3&                           dir,
			float                                 length,
			Hit*                                  outHit,
			const std::vector<RegisteredBVH>&     bvhSet,
			const std::vector<Unnamed::Triangle>& allTriangles
		) {
			// まずは各BVHのルートのAABBとレイが交差するかを確認
			// してなきゃ意味ないからね! これが噂のブロードフェーズ!
			std::vector<const RegisteredBVH*> filtered;
			const Unnamed::Ray                broadRay = {
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
				Unnamed::AABB root = cast.ExpandNode(bvh.nodes[0].bounds);
				float         t    = length;
				if (RayVsAABB(broadRay, root, t)) {
					filtered.emplace_back(&bvh);
				}
			}

			if (filtered.empty()) {
				// 交差しなかった...
				return false;
			}

			// 本格的に探索する
			// 一番近い衝突のTOI (TOI: Time of Impact 衝突までの時間[0.0f ～ 1.0f])
			float    bestTOI       = 1.0f;
			uint32_t hitTri        = UINT32_MAX; // ヒットした三角形のインデックス
			uint64_t hitEntityGuid = 0;
			Vec3     hitNormal; // ヒットした法線
			uint32_t stack[64]; // スタックを使ってBVHを探索(深さ優先探索)

			// ブロードフェーズで検知されたBVHを探索する
			for (auto* bvh : filtered) {
				int sp      = 0;
				stack[sp++] = 0; // ルートノードからスタート

				while (sp) {
					const uint32_t index = stack[--sp];
					const auto&    node  = bvh->nodes[index];

#ifdef _DEBUG
					Vec3 center = (node.bounds.min + node.bounds.max) *
					              0.5f;
					const Vec3 size = node.bounds.max - node.bounds.min;

					// TODO: デバッグ描画はエンジンの外に出すべきかもね
					// DebugDraw::DrawBox(
					// 	center,
					// 	Quaternion::identity,
					// 	size,
					// 	Vec4::orange
					// );
#endif

					// 現在の最良TOIを使った早期終了
					Unnamed::Ray pruneRay = broadRay;
					pruneRay.tMax         = bestTOI * length;
					float tBox            = bestTOI * length;
					if (
						!RayVsAABB(
							pruneRay,
							cast.ExpandNode(node.bounds),
							tBox
						)
					) {
						continue; // 残念!
					}

					if (node.primCount == 0) {
						// 近い子ノードを探索する
						stack[sp++] = node.leftFirst;
						stack[sp++] = node.rightFirst;
					} else {
						uint32_t first = node.leftFirst;
						for (uint32_t i = 0; i < node.primCount; ++i) {
							uint32_t triIdx = bvh->triIndices[first + i];
							float    toi;
							Vec3     nrm;
							if (
								cast.TestTriangle(
									allTriangles[triIdx], dir, length,
									toi, nrm
								)
							) {
								if (toi < bestTOI) {
									bestTOI       = toi;
									hitTri        = triIdx;
									hitEntityGuid = bvh->ownerGuid;
									hitNormal     = nrm;
									// TOIが更新されたら、より厳しい早期終了を設定
									pruneRay.tMax = bestTOI * length;
								}
							}
						}
					}
				}
			}

			if (hitTri == UINT32_MAX) {
				return false; // 残念!
			}
			if (outHit) {
				Vec3        finalNormal = hitNormal;
				const float nLenSq      = finalNormal.SqrLength();
				if (nLenSq > 1e-12f) {
					finalNormal /= std::sqrt(nLenSq);
				} else {
					finalNormal = Vec3::zero;
				}
				outHit->t               = bestTOI;
				outHit->normal          = finalNormal;
				outHit->pos             = cast.ComputeImpactPoint(
					start,
					dirNormalized,
					length,
					bestTOI,
					finalNormal
				);
				outHit->triIndex      = hitTri;
				outHit->hitEntityGuid = hitEntityGuid;
			}
			return true;
		}

		static void AddGlobalOffset(
			std::vector<uint32_t>& indices,
			uint32_t               base
		);

		std::vector<Unnamed::Triangle> mTriangles;
		std::vector<FlatNode>          mNodes;
		std::vector<uint32_t>          mTriIndices;

		std::vector<RegisteredBVH> mBVHs;
	};
}
