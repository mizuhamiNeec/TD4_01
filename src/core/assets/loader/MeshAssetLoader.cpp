#include "MeshAssetLoader.h"

#include <array>
#include <filesystem>
#include <unordered_map>

#include <assimp/Importer.hpp>
#include <assimp/matrix4x4.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "core/assets/types/MeshAssetData.h"
#include "core/string/StrUtil.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "MeshAssetLdr";

		constexpr std::array kSupportedExtensions = {
			".obj",
			".fbx",
			".gltf",
			".glb",
		};

		Mat4 ToMat4(const aiMatrix4x4& m) {
			Mat4 out    = Mat4::identity;
			out.m[0][0] = m.a1;
			out.m[0][1] = m.a2;
			out.m[0][2] = m.a3;
			out.m[0][3] = m.a4;
			out.m[1][0] = m.b1;
			out.m[1][1] = m.b2;
			out.m[1][2] = m.b3;
			out.m[1][3] = m.b4;
			out.m[2][0] = m.c1;
			out.m[2][1] = m.c2;
			out.m[2][2] = m.c3;
			out.m[2][3] = m.c4;
			out.m[3][0] = m.d1;
			out.m[3][1] = m.d2;
			out.m[3][2] = m.d3;
			out.m[3][3] = m.d4;
			return out;
		}

		void AddBoneWeight(
			MeshVertex& v, const uint16_t boneIndex, const float weight
		) {
			for (int i = 0; i < 4; ++i) {
				if (v.boneWeights[i] == 0.0f) {
					v.boneIndices[i] = boneIndex;
					v.boneWeights[i] = weight;
					return;
				}
			}

			// 4本超えた場合は最小重みを置き換える
			int minIndex = 0;
			for (int i = 1; i < 4; ++i) {
				if (v.boneWeights[i] < v.boneWeights[minIndex]) {
					minIndex = i;
				}
			}
			if (weight > v.boneWeights[minIndex]) {
				v.boneIndices[minIndex] = boneIndex;
				v.boneWeights[minIndex] = weight;
			}
		}
	}

	bool MeshAssetLoader::CanLoad(
		const std::string_view path, ASSET_TYPE* outType
	) const {
		const std::string ext = StrUtil::ToLowerExt(path);
		bool              ok  = false;

		for (const auto& supportedExt : kSupportedExtensions) {
			if (ext == supportedExt) {
				ok = true;
				break;
			}
		}

		if (outType) { *outType = ok ? ASSET_TYPE::MESH : ASSET_TYPE::UNKNOWN; }

		return ok;
	}

	LoadResult MeshAssetLoader::Load(const std::string& path) {
		LoadResult r = {};

		Assimp::Importer   importer;
		constexpr uint32_t kBaseFlags =
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_ImproveCacheLocality |
			aiProcess_SortByPType |
			aiProcess_GenSmoothNormals |
			aiProcess_ConvertToLeftHanded;

		const aiScene* scene = importer.ReadFile(
			path,
			kBaseFlags
		);

		if (!scene) {
			Error(
				kChannel, "メッシュの読み込みに失敗しました: path={}, err={}",
				path, importer.GetErrorString()
			);
			return r;
		}

		if (!scene->HasMeshes()) {
			Error(kChannel, "メッシュが含まれていません: {}", path);
			return r;
		}

		bool hasBones = false;
		for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
			if (scene->mMeshes[i] && scene->mMeshes[i]->HasBones()) {
				hasBones = true;
				break;
			}
		}

		// スキン無しメッシュは従来どおりプリトランスフォームで使う
		if (!hasBones) {
			scene = importer.ReadFile(
				path, kBaseFlags | aiProcess_PreTransformVertices
			);
			if (!scene) {
				Error(
					kChannel, "メッシュの再読み込みに失敗しました: path={}, err={}",
					path, importer.GetErrorString()
				);
				return r;
			}
		}

		MeshAssetData out = {};
		out.sourcePath    = path;
		out.hasSkinning   = hasBones;
		std::unordered_map<std::string, uint16_t> boneNameToIndex;

		for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++
		     meshIndex) {
			const aiMesh* mesh = scene->mMeshes[meshIndex];
			if (!mesh || !(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE)) {
				continue;
			}

			const uint32_t baseVertex = static_cast<uint32_t>(out.vertices.
				size());
			out.vertices.reserve(out.vertices.size() + mesh->mNumVertices);

			for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
				MeshVertex v{};
				v.position = Vec3(
					mesh->mVertices[i].x,
					mesh->mVertices[i].y,
					mesh->mVertices[i].z
				);
				v.normal = mesh->HasNormals() ?
					           Vec3(
						           mesh->mNormals[i].x,
						           mesh->mNormals[i].y,
						           mesh->mNormals[i].z
					           ) :
					           Vec3::up;
				v.uv = mesh->HasTextureCoords(0) ?
					       Vec2(
						       mesh->mTextureCoords[0][i].x,
						       mesh->mTextureCoords[0][i].y
					       ) :
					       Vec2::zero;

				out.localBoundsMin = Vec3::Min(out.localBoundsMin, v.position);
				out.localBoundsMax = Vec3::Max(out.localBoundsMax, v.position);

				out.vertices.emplace_back(v);
			}

			if (mesh->HasBones()) {
				for (uint32_t boneIdx = 0; boneIdx < mesh->mNumBones; ++
				     boneIdx) {
					const aiBone* bone = mesh->mBones[boneIdx];
					if (!bone) { continue; }

					const std::string boneName        = bone->mName.C_Str();
					uint16_t          globalBoneIndex = 0;

					const auto it = boneNameToIndex.find(boneName);
					if (it == boneNameToIndex.end()) {
						globalBoneIndex = static_cast<uint16_t>(out.skeleton.
							size());
						boneNameToIndex.emplace(boneName, globalBoneIndex);

						SkeletonBoneAssetData sb = {};
						sb.name                  = boneName;
						sb.parentIndex           = -1;
						sb.inverseBindPose       = ToMat4(bone->mOffsetMatrix);
						out.skeleton.emplace_back(std::move(sb));
					} else { globalBoneIndex = it->second; }

					for (uint32_t w = 0; w < bone->mNumWeights; ++w) {
						const uint32_t localVertex = bone->mWeights[w].
							mVertexId;
						if (localVertex >= mesh->mNumVertices) { continue; }
						const uint32_t globalVertex = baseVertex + localVertex;
						if (globalVertex >= out.vertices.size()) { continue; }
						AddBoneWeight(
							out.vertices[globalVertex],
							globalBoneIndex,
							bone->mWeights[w].mWeight
						);
					}
				}
			}

			out.indices.reserve(out.indices.size() + mesh->mNumFaces * 3);
			for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++
			     faceIndex) {
				const aiFace& face = mesh->mFaces[faceIndex];
				if (face.mNumIndices < 3) { continue; }

				for (uint32_t i = 1; i + 1 < face.mNumIndices; ++i) {
					out.indices.emplace_back(baseVertex + face.mIndices[0]);
					out.indices.emplace_back(baseVertex + face.mIndices[i]);
					out.indices.emplace_back(baseVertex + face.mIndices[i + 1]);
				}
			}
		}

		if (out.vertices.empty() || out.indices.empty()) {
			Error(kChannel, "有効な頂点/インデックスがありません: {}", path);
			return r;
		}

		r.payload     = std::move(out);
		r.resolveName = std::filesystem::path(path).filename().string();

		std::error_code ec;
		if (std::filesystem::exists(path, ec)) {
			r.stamp.sizeInBytes = std::filesystem::file_size(path, ec);
		}

		return r;
	}
}
