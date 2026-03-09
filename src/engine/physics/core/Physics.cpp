#include "Physics.h"

#include <algorithm>
#include <chrono>
#include <pch.h>
#include <vector>

#include <engine/unnamed/physics/BoxCast.h>
#include <engine/unnamed/physics/PhysicsTypes.h>
#include <engine/unnamed/physics/RayCast.h>
#include <engine/unnamed/physics/SphereCast.h>
#include <engine/unnamed/subsystem/console/Log.h>

namespace Unnamed::Physics {
	namespace {
		Unnamed::Triangle BuildTriangle(
			const Unnamed::MeshVertex& a,
			const Unnamed::MeshVertex& b,
			const Unnamed::MeshVertex& c,
			const Mat4&                world
		) {
			return {
				.v0 = world.TransformPoint(a.position),
				.v1 = world.TransformPoint(b.position),
				.v2 = world.TransformPoint(c.position),
			};
		}
	}

	/// @brief 初期化
	void Engine::Init() {}

	/// @brief 更新
	void Engine::Update(float) const {
#ifdef _DEBUG
		// const auto camera = CameraManager::GetActiveCamera();
		// if (camera) {
		// 	Mat4       invView = camera->GetViewMat().Inverse();
		// 	const Vec3 start   = invView.GetTranslate();
		// 	Vec3       dir     = invView.GetForward();
		//
		// 	dir.Normalize();
		// 	const Unnamed::Ray ray = {
		// 		.origin = start,
		// 		.dir    = dir,
		// 		.invDir = 1.0f / dir,
		// 		.tMin   = 0.0f,
		// 		.tMax   = 1e30f
		// 	};
		//
		// 	DebugDraw::DrawAxis(
		// 		start,
		// 		Quaternion::identity
		// 	);
		//
		// 	Hit hit;
		// 	if (RayCast(ray, &hit)) {
		// 		DebugDraw::DrawRay(
		// 			start,
		// 			dir * hit.t,
		// 			Vec4::blue
		// 		);
		// 		DebugDraw::DrawAxis(
		// 			hit.pos,
		// 			Quaternion::identity
		// 		);
		// 		DebugDraw::DrawRay(
		// 			hit.pos,
		// 			hit.normal,
		// 			Vec4::magenta
		// 		);
		// 	}
		// }
#endif
	}

	/// @brief レイキャストを行う関数
	/// @param ray レイ情報
	/// @param outHit 衝突情報の出力先
	/// @return 衝突した場合trueを返す
	bool Engine::RayCast(const Unnamed::Ray& ray, Hit* outHit) const {
		Unnamed::Physics::RayCast cast;
		cast.start  = ray.origin;
		cast.invDir = ray.invDir;
		return CastBVH(
			cast, ray.origin, ray.dir, ray.tMax, outHit, mBVHs,
			mTriangles
		);
	}

	/// @brief ボックスキャストを行う関数
	/// @param box ボックス情報
	/// @param dir キャスト方向
	/// @param length キャスト距離
	/// @param outHit 衝突情報の出力先
	/// @return 衝突した場合trueを返す
	bool Engine::BoxCast(
		const Unnamed::Box& box,
		const Vec3&         dir,
		const float         length,
		Hit*                outHit
	) const {
		Vec3  dirN = dir;
		float len  = length;

		const float dirLen = dirN.Length();
		if (dirLen > 1e-6f) {
			dirN /= dirLen;
			if (fabs(len - dirLen) < 1e-4f) len = dirLen;
		} else {
			return false; // ゼロ方向なら衝突無し
		}

		Unnamed::Physics::BoxCast caster;
		caster.box  = box;
		caster.half = box.halfSize;

		return CastBVH(
			caster,
			box.center,
			dirN,
			len,
			outHit,
			mBVHs, mTriangles
		);
	}

	/// @brief スフィアキャストを行う関数
	/// @param start スフィアの開始位置
	/// @param radius スフィアの半径
	/// @param dir キャスト方向
	/// @param length キャスト距離
	/// @param outHit 衝突情報の出力先
	/// @return 衝突した場合trueを返す
	bool Engine::SphereCast(
		const Vec3& start,
		const float radius,
		const Vec3& dir,
		const float length,
		Hit*        outHit
	) const {
		Unnamed::Physics::SphereCast cast;
		cast.center = start;
		cast.radius = radius;

		return CastBVH(cast, start, dir, length, outHit, mBVHs, mTriangles);
	}

	/// @brief ボックスとメッシュの重なり判定を行う関数
	/// @param box ボックス情報
	/// @param outHit 衝突情報の出力先
	/// @return 重なりがあった場合trueを返す
	bool Engine::BoxOverlap(
		const Unnamed::Box& box,
		Hit*                outHit
	) const {
		if (mBVHs.empty() || mTriangles.empty()) {
			return false;
		}

		// ブロードフェーズ：ボックスのAABBと各BVHのルートAABBの重なりをチェック
		std::vector<const RegisteredBVH*> filtered;
		Unnamed::AABB                     boxAABB;
		boxAABB.min = box.center - box.halfSize;
		boxAABB.max = box.center + box.halfSize;

		for (const auto& bvh : mBVHs) {
			const Unnamed::AABB& rootBounds = bvh.nodes[0].bounds;
			// AABB同士の重なり判定
			if (boxAABB.max.x >= rootBounds.min.x && boxAABB.min.x <= rootBounds
			    .max.x &&
			    boxAABB.max.y >= rootBounds.min.y && boxAABB.min.y <= rootBounds
			    .max.y &&
			    boxAABB.max.z >= rootBounds.min.z && boxAABB.min.z <= rootBounds
			    .max.z) {
				filtered.emplace_back(&bvh);
			}
		}

		if (filtered.empty()) {
			return false;
		}

		// ナローフェーズ：詳細な重なり判定
		float    minPenetration = FLT_MAX;
		uint32_t hitTri         = UINT32_MAX;
		Vec3     hitNormal;
		Vec3     hitPos;
		uint32_t stack[64];
		uint64_t hitEntityGuid = 0;

		for (auto* bvh : filtered) {
			int sp      = 0;
			stack[sp++] = 0; // ルートノードからスタート

			while (sp) {
				const uint32_t index = stack[--sp];
				const auto&    node  = bvh->nodes[index];

				// ノードのAABBとボックスの重なり判定
				if (!(boxAABB.max.x >= node.bounds.min.x && boxAABB.min.x <=
				      node.bounds.max.x &&
				      boxAABB.max.y >= node.bounds.min.y && boxAABB.min.y <=
				      node.
				      bounds.max.y &&
				      boxAABB.max.z >= node.bounds.min.z && boxAABB.min.z <=
				      node.
				      bounds.max.z)) {
					continue; // 重なりなし
				}

				if (node.primCount == 0) {
					// 内部ノード：子ノードをスタックに追加
					stack[sp++] = node.leftFirst;
					stack[sp++] = node.rightFirst;
				} else {
					// 葉ノード：三角形との詳細判定
					const uint32_t first = node.leftFirst;
					for (uint32_t i = 0; i < node.primCount; ++i) {
						const uint32_t triIdx = bvh->triIndices[first + i];
						const Unnamed::Triangle& tri = mTriangles[triIdx];

						Vec3  separationAxis;
						float penetrationDepth;
						if (BoxVsTriangleOverlap(
							box, tri, separationAxis, penetrationDepth
						)) {
							if (penetrationDepth < minPenetration) {
								minPenetration = penetrationDepth;
								hitTri = triIdx;
								hitNormal = separationAxis; // already outward
								hitPos = box.center + hitNormal * (std::min(
										         {
											         box.halfSize.x,
											         box.halfSize.y,
											         box.halfSize.z
										         }
									         ) - penetrationDepth * 0.5f);
								hitEntityGuid = bvh->ownerGuid;
							}
						}
					}
				}
			}
		}

		if (hitTri == UINT32_MAX) {
			return false; // 重なりなし
		}

		if (outHit) {
			outHit->t             = 1.0f;           // sweep 用でないので 1
			outHit->depth         = minPenetration; // ← depth をセット
			outHit->pos           = hitPos;
			outHit->normal        = hitNormal;
			outHit->triIndex      = hitTri;
			outHit->hitEntityGuid = hitEntityGuid;
		}
		return true;
	}


	int Engine::BoxOverlap(
		const Unnamed::Box& box,
		Hit*                outHits,
		const int           maxHits
	) const {
		int hitCount = 0;
		if (mBVHs.empty() || mTriangles.empty() || maxHits <= 0) {
			return hitCount;
		}

		// ブロードフェーズ：ボックスのAABBと各BVHのルートAABBの重なりをチェック
		std::vector<const RegisteredBVH*> filtered;
		Unnamed::AABB                     boxAABB;
		boxAABB.min = box.center - box.halfSize;
		boxAABB.max = box.center + box.halfSize;

		for (const auto& bvh : mBVHs) {
			const Unnamed::AABB& rootBounds = bvh.nodes[0].bounds;
			// AABB同士の重なり判定
			if (boxAABB.max.x >= rootBounds.min.x && boxAABB.min.x <= rootBounds
			    .max.x &&
			    boxAABB.max.y >= rootBounds.min.y && boxAABB.min.y <= rootBounds
			    .max.y &&
			    boxAABB.max.z >= rootBounds.min.z && boxAABB.min.z <= rootBounds
			    .max.z) {
				filtered.emplace_back(&bvh);
			}
		}

		if (filtered.empty()) {
			return hitCount; // 重なりなし
		}

		// ナローフェーズ：詳細な重なり判定
		uint32_t stack[64];

		for (auto* bvh : filtered) {
			int sp      = 0;
			stack[sp++] = 0; // ルートノードからスタート

			while (sp && hitCount < maxHits) {
				const uint32_t index = stack[--sp];
				const auto&    node  = bvh->nodes[index];

				// ノードのAABBとボックスの重なり判定
				if (!(boxAABB.max.x >= node.bounds.min.x && boxAABB.min.x <=
				      node.bounds.max.x &&
				      boxAABB.max.y >= node.bounds.min.y && boxAABB.min.y <=
				      node.
				      bounds.max.y &&
				      boxAABB.max.z >= node.bounds.min.z && boxAABB.min.z <=
				      node.
				      bounds.max.z)) {
					continue; // 重なりなし
				}

				if (node.primCount == 0) {
					// 内部ノード：子ノードをスタックに追加
					stack[sp++] = node.leftFirst;
					stack[sp++] = node.rightFirst;
				} else {
					// 葉ノード：三角形との詳細判定
					const uint32_t first = node.leftFirst;
					for (uint32_t i = 0; i < node.primCount && hitCount <
					                     maxHits; ++i) {
						const uint32_t triIdx = bvh->triIndices[first + i];
						const Unnamed::Triangle& tri = mTriangles[triIdx];

						Vec3  separationAxis;
						float penetrationDepth;
						if (BoxVsTriangleOverlap(
							box, tri, separationAxis, penetrationDepth
						)) {
							Hit tmpHit;
							tmpHit.t     = 1.0f;
							tmpHit.depth = penetrationDepth;
							tmpHit.pos   = box.center + separationAxis * (
								             std::min(
									             {
										             box.halfSize.x,
										             box.halfSize.y,
										             box.halfSize.z
									             }
								             ) - penetrationDepth * 0.5f);
							tmpHit.normal        = separationAxis;
							tmpHit.triIndex      = triIdx;
							tmpHit.hitEntityGuid = bvh->ownerGuid;

							outHits[hitCount] = tmpHit;
							hitCount++;
						}
					}
				}
			}
		}

		return hitCount;
	}

	// --- Static mesh registration (single definition) ---
	void Engine::ClearStaticMeshes() {
		mTriangles.clear();
		mNodes.clear();
		mTriIndices.clear();
		mBVHs.clear();
	}

	bool Engine::RegisterStaticMesh(
		const uint64_t                             ownerGuid,
		const std::span<const Unnamed::MeshVertex> vertices,
		const std::span<const uint32_t>            indices,
		const Mat4&                                world
	) {
		if (ownerGuid == 0 || vertices.empty() || indices.size() < 3) {
			return false;
		}

		const auto start = std::chrono::steady_clock::now();

		const size_t                   triStart = mTriangles.size();
		std::vector<Unnamed::Triangle> localTriangles;
		localTriangles.reserve(indices.size() / 3);

		const auto buildTrisStart = std::chrono::steady_clock::now();
		for (size_t i = 0; i + 2 < indices.size(); i += 3) {
			const uint32_t i0 = indices[i + 0];
			const uint32_t i1 = indices[i + 1];
			const uint32_t i2 = indices[i + 2];
			if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices
			    .size()) {
				continue;
			}
			localTriangles.emplace_back(
				BuildTriangle(vertices[i0], vertices[i1], vertices[i2], world)
			);
		}
		const auto buildTrisEnd = std::chrono::steady_clock::now();

		if (localTriangles.empty()) {
			return false;
		}

		const auto insertStart = std::chrono::steady_clock::now();
		mTriangles.insert(
			mTriangles.end(),
			localTriangles.begin(),
			localTriangles.end()
		);
		const auto insertEnd = std::chrono::steady_clock::now();

		const auto            bvhStart = std::chrono::steady_clock::now();
		BVHBuilder            builder;
		std::vector<FlatNode> nodes;
		std::vector<uint32_t> triIndices;
		builder.Build(localTriangles, nodes, triIndices);
		AddGlobalOffset(triIndices, static_cast<uint32_t>(triStart));
		const auto bvhEnd = std::chrono::steady_clock::now();

		RegisteredBVH registered = {};
		registered.nodes         = std::move(nodes);
		registered.triIndices    = std::move(triIndices);
		registered.triStart      = triStart;
		registered.triCount      = localTriangles.size();
		registered.ownerGuid     = ownerGuid;
		mBVHs.emplace_back(std::move(registered));

		const auto end = std::chrono::steady_clock::now();
		Msg(
			"Physics",
			"RegisterStaticMesh timing: buildTris={}ms insert={}ms bvhBuild={}ms total={}ms (ownerGuid={} verts={} idx={} tris={})",
			std::chrono::duration_cast<std::chrono::milliseconds>(
				buildTrisEnd - buildTrisStart
			).count(),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				insertEnd - insertStart
			).count(),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				bvhEnd - bvhStart
			).count(),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				end - start
			).count(),
			ownerGuid,
			vertices.size(),
			indices.size(),
			localTriangles.size()
		);

		return true;
	}

	void Engine::UnregisterStaticMesh(const uint64_t ownerGuid) {
		if (ownerGuid == 0) {
			return;
		}
		const auto it = std::find_if(
			mBVHs.begin(),
			mBVHs.end(),
			[ownerGuid](const RegisteredBVH& bvh) {
				return bvh.ownerGuid == ownerGuid;
			}
		);
		if (it == mBVHs.end()) {
			return;
		}

		ClearStaticMeshes();
	}

	/// @brief インデックスにグローバルオフセットを追加します
	/// @param indices インデックス配列
	/// @param base オフセット値
	void Engine::AddGlobalOffset(
		std::vector<uint32_t>& indices,
		const uint32_t         base
	) {
		for (uint32_t& index : indices) {
			index += base;
		}
	}
}
