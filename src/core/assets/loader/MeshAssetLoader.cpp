#include "MeshAssetLoader.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include <assimp/Importer.hpp>
#include <assimp/matrix4x4.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "core/assets/types/MeshAssetData.h"
#include "core/string/StrUtil.h"

#include "engine/profiler/Profiler.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel          = "MeshAssetLdr";
		constexpr uint32_t         kMeshCacheMagic   = 0x48534D55; // UMSH
		constexpr uint32_t         kMeshCacheVersion = 1;

		constexpr std::array kSupportedExtensions = {
			".obj",
			".fbx",
			".gltf",
			".glb",
		};

		struct MeshCacheHeader {
			uint32_t magic                = kMeshCacheMagic;
			uint32_t version              = kMeshCacheVersion;
			uint64_t sourcePathHash       = 0;
			int64_t  sourceLastWriteTicks = 0;
			uint64_t sourceSizeBytes      = 0;
			uint64_t importerConfigHash   = 0;
			uint32_t vertexCount          = 0;
			uint32_t indexCount           = 0;
			uint32_t skeletonBoneCount    = 0;
			uint32_t flags                = 0;
		};

		FileStamp ReadCurrentFileStamp(const std::string& path) {
			FileStamp       stamp = {};
			std::error_code ec;
			if (!std::filesystem::exists(path, ec)) {
				return stamp;
			}

			const auto lastWrite = std::filesystem::last_write_time(path, ec);
			if (!ec) {
				stamp.lastWriteTicks = lastWrite.time_since_epoch().count();
			}
			stamp.sizeInBytes = std::filesystem::file_size(path, ec);
			return stamp;
		}

		uint64_t HashCombine64(const uint64_t a, const uint64_t b) {
			return a ^ b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2);
		}

		uint64_t BuildImporterConfigHash(const bool hasSkinning) {
			constexpr uint64_t kBaseFlags =
				aiProcess_JoinIdenticalVertices |
				aiProcess_ImproveCacheLocality |
				aiProcess_SortByPType |
				aiProcess_ConvertToLeftHanded;
			uint64_t hash = HashCombine64(kBaseFlags, kMeshCacheVersion);
			if (!hasSkinning) {
				hash = HashCombine64(hash, aiProcess_PreTransformVertices);
			}
			return hash;
		}

		template <typename T>
		bool WritePod(std::ofstream& ofs, const T& value) {
			ofs.write(reinterpret_cast<const char*>(&value), sizeof(T));
			return static_cast<bool>(ofs);
		}

		template <typename T>
		bool ReadPod(std::ifstream& ifs, T& value) {
			ifs.read(reinterpret_cast<char*>(&value), sizeof(T));
			return static_cast<bool>(ifs);
		}

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

		/// @brief スキン無しメッシュの頂点に、階層トランスフォームを焼き込む
		/// Assimpのシーン構造を再帰的にたどり、各ノードの変換を子ノードとメッシュ頂点に適用する。
		void BakeNodeTransformsIntoMesh(
			const aiNode*      node,
			const aiMatrix4x4& parentTransform,
			const aiScene*     scene
		) {
			if (!node || !scene) {
				return;
			}

			const aiMatrix4x4 global = parentTransform * node->mTransformation;

			for (uint32_t mi = 0; mi < node->mNumMeshes; ++mi) {
				const uint32_t meshIndex = node->mMeshes[mi];
				if (meshIndex >= scene->mNumMeshes) {
					continue;
				}
				const aiMesh* mesh = scene->mMeshes[meshIndex];
				if (!mesh) {
					continue;
				}

				// スキン無しだけで呼ばれる想定だが、混在ファイルの安全のためボーン付きは触らない
				if (mesh->HasBones()) {
					continue;
				}

				// 位置
				for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
					mesh->mVertices[v] = global * mesh->mVertices[v];
				}

				// 法線（方向ベクトルとして変換）
				if (mesh->HasNormals()) {
					aiMatrix4x4 rotScale = global;
					rotScale.a4          = rotScale.b4 = rotScale.c4 = 0.0f;
					// translation 제거
					for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
						mesh->mNormals[v] = rotScale * mesh->mNormals[v];
						mesh->mNormals[v].Normalize();
					}
				}
			}

			for (uint32_t c = 0; c < node->mNumChildren; ++c) {
				BakeNodeTransformsIntoMesh(node->mChildren[c], global, scene);
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

		if (outType) {
			*outType = ok ? ASSET_TYPE::MESH : ASSET_TYPE::UNKNOWN;
		}

		return ok;
	}

	LoadResult MeshAssetLoader::Load(const std::string& path) {
		LoadResult r = {};
		if (TryLoadDerivedCache(path, r)) {
			return r;
		}

		Profiler*            profiler = ServiceLocator::Get<Profiler>();
		Profiler::ScopeTimer assimpScope(profiler, "MeshImport.Assimp");

		Assimp::Importer importer;
		// ここでは ReadFile を1回に統一する。
		// 以前はスキン無しのとき aiProcess_PreTransformVertices を付けて再読込していたが、
		// 代わりにローダ側でノード変換を焼き込む。
		constexpr uint32_t kBaseFlags =
			aiProcess_ImproveCacheLocality |
			aiProcess_ConvertToLeftHanded;

		const aiScene* scene = importer.ReadFile(path, kBaseFlags);

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

		// スキン無しメッシュは従来どおり「階層トランスフォームを焼き込んだ頂点」を使いたいので、
		// Importer内部のシーンを直接変換する（ReadFileの二重呼び出しを回避）。
		if (!hasBones && scene->mRootNode) {
			BakeNodeTransformsIntoMesh(scene->mRootNode, aiMatrix4x4(), scene);
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
			out.vertices.reserve(
				out.vertices.size() + static_cast<size_t>(mesh->mNumVertices)
			);

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
					if (!bone) {
						continue;
					}

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
					} else {
						globalBoneIndex = it->second;
					}

					for (uint32_t w = 0; w < bone->mNumWeights; ++w) {
						const uint32_t localVertex = bone->mWeights[w].
							mVertexId;
						if (localVertex >= mesh->mNumVertices) {
							continue;
						}
						const uint32_t globalVertex = baseVertex + localVertex;
						if (globalVertex >= out.vertices.size()) {
							continue;
						}
						AddBoneWeight(
							out.vertices[globalVertex],
							globalBoneIndex,
							bone->mWeights[w].mWeight
						);
					}
				}
			}

			out.indices.reserve(
				out.indices.size() + static_cast<size_t>(mesh->mNumFaces) * 3u
			);
			for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++
			     faceIndex) {
				const aiFace& face = mesh->mFaces[faceIndex];
				if (face.mNumIndices < 3) {
					continue;
				}

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
		r.stamp       = ReadCurrentFileStamp(path);
		WriteDerivedCache(path, r);

		return r;
	}

	std::filesystem::path MeshAssetLoader::GetDerivedCachePath(
		const std::string& sourcePath
	) const {
		const std::string normalized = StrUtil::NormalizePath(sourcePath);
		const uint64_t    hash       = std::hash<std::string>{}(normalized);
		return std::filesystem::path("./bin/cache/assets/meshes") /
		       (std::to_string(hash) + ".umeshbin");
	}

	bool MeshAssetLoader::TryLoadDerivedCache(
		const std::string& path, LoadResult& out
	) {
		const std::filesystem::path cachePath = GetDerivedCachePath(path);
		if (!std::filesystem::exists(cachePath)) {
			return false;
		}

		std::ifstream ifs(cachePath, std::ios::binary);
		if (!ifs) {
			return false;
		}

		MeshCacheHeader header = {};
		if (!ReadPod(ifs, header)) {
			return false;
		}
		if (header.magic != kMeshCacheMagic || header.version !=
		    kMeshCacheVersion) {
			return false;
		}

		const std::string normalized  = StrUtil::NormalizePath(path);
		const FileStamp   sourceStamp = ReadCurrentFileStamp(path);
		if (header.sourcePathHash != std::hash<std::string>{}(normalized)) {
			return false;
		}
		if (
			header.sourceLastWriteTicks !=
			sourceStamp.lastWriteTicks ||
			header.sourceSizeBytes != sourceStamp.sizeInBytes
		) {
			return false;
		}

		const bool hasSkinning = (header.flags & 0x1u) != 0u;
		if (header.importerConfigHash != BuildImporterConfigHash(hasSkinning)) {
			return false;
		}

		MeshAssetData mesh = {};
		mesh.sourcePath    = path;
		mesh.hasSkinning   = hasSkinning;
		mesh.vertices.resize(header.vertexCount);
		mesh.indices.resize(header.indexCount);
		mesh.skeleton.resize(header.skeletonBoneCount);

		if (
			(header.vertexCount > 0 &&
			 !static_cast<bool>(ifs.read(
				 reinterpret_cast<char*>(mesh.vertices.data()),
				 sizeof(MeshVertex) * mesh.vertices.size()
			 ))) ||
			(header.indexCount > 0 &&
			 !static_cast<bool>(ifs.read(
				 reinterpret_cast<char*>(mesh.indices.data()),
				 sizeof(uint32_t) * mesh.indices.size()
			 )))
		) {
			return false;
		}

		for (uint32_t i = 0; i < header.skeletonBoneCount; ++i) {
			uint32_t nameLen = 0;
			if (!ReadPod(ifs, nameLen)) {
				return false;
			}
			mesh.skeleton[i].name.resize(nameLen);
			if (
				nameLen > 0 &&
				!static_cast<bool>(ifs.read(
					mesh.skeleton[i].name.data(), nameLen
				))
			) {
				return false;
			}
			if (!ReadPod(ifs, mesh.skeleton[i].parentIndex)) {
				return false;
			}
			if (!ReadPod(ifs, mesh.skeleton[i].inverseBindPose)) {
				return false;
			}
		}

		for (const auto& vertex : mesh.vertices) {
			mesh.localBoundsMin = Vec3::Min(
				mesh.localBoundsMin, vertex.position
			);
			mesh.localBoundsMax = Vec3::Max(
				mesh.localBoundsMax, vertex.position
			);
		}

		out.payload     = std::move(mesh);
		out.resolveName = std::filesystem::path(path).filename().string();
		out.stamp       = sourceStamp;

		if (Profiler* profiler = ServiceLocator::Get<Profiler>()) {
			profiler->AddSample("MeshImport.CacheRead", 1.0f);
		}
		return true;
	}

	bool MeshAssetLoader::WriteDerivedCache(
		const std::string& path, const LoadResult& in
	) {
		const auto* mesh = std::get_if<MeshAssetData>(
			&const_cast<AssetPayload&>(in.payload)
		);
		if (!mesh) {
			return false;
		}

		const std::filesystem::path cachePath = GetDerivedCachePath(path);
		std::error_code             ec;
		std::filesystem::create_directories(cachePath.parent_path(), ec);

		std::ofstream ofs(cachePath, std::ios::binary | std::ios::trunc);
		if (!ofs) {
			return false;
		}

		MeshCacheHeader header = {};
		header.sourcePathHash  = std::hash<std::string>{}(
			StrUtil::NormalizePath(path)
		);
		header.sourceLastWriteTicks = in.stamp.lastWriteTicks;
		header.sourceSizeBytes = in.stamp.sizeInBytes;
		header.importerConfigHash = BuildImporterConfigHash(mesh->hasSkinning);
		header.vertexCount = static_cast<uint32_t>(mesh->vertices.size());
		header.indexCount = static_cast<uint32_t>(mesh->indices.size());
		header.skeletonBoneCount = static_cast<uint32_t>(mesh->skeleton.size());
		header.flags = mesh->hasSkinning ? 0x1u : 0u;

		if (!WritePod(ofs, header)) {
			return false;
		}
		if (
			(header.vertexCount > 0 &&
			 !static_cast<bool>(ofs.write(
				 reinterpret_cast<const char*>(mesh->vertices.data()),
				 sizeof(MeshVertex) * mesh->vertices.size()
			 ))) ||
			(header.indexCount > 0 &&
			 !static_cast<bool>(ofs.write(
				 reinterpret_cast<const char*>(mesh->indices.data()),
				 sizeof(uint32_t) * mesh->indices.size()
			 )))
		) {
			return false;
		}

		for (const auto& bone : mesh->skeleton) {
			const uint32_t nameLen = static_cast<uint32_t>(bone.name.size());
			if (!WritePod(ofs, nameLen)) {
				return false;
			}
			if (
				nameLen > 0 &&
				!static_cast<bool>(ofs.write(bone.name.data(), nameLen))
			) {
				return false;
			}
			if (!WritePod(ofs, bone.parentIndex)) {
				return false;
			}
			if (!WritePod(ofs, bone.inverseBindPose)) {
				return false;
			}
		}

		if (Profiler* profiler = ServiceLocator::Get<Profiler>()) {
			profiler->AddSample("MeshImport.CacheWrite", 1.0f);
		}
		return static_cast<bool>(ofs);
	}
}
