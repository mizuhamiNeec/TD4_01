#include "ParticleEmitterComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>

#ifdef _DEBUG
#include <imgui.h>
#endif

#include "core/string/StrUtil.h"

#include "../TransformComponent.h"

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/AssetType.h"
#include "core/io/json/JsonReader.h"
#include "core/io/json/JsonWriter.h"

#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kLogChannel = "ParticleEmitterComponent";
		constexpr std::string_view kPresetDirectory = "Resources/Particle";

		[[nodiscard]] bool IsCurrentDirectoryRelativePath(
			const std::string_view path
		) {
			return path.starts_with("./") || path.starts_with("../");
		}

		[[nodiscard]] bool IsEngineRootRelativePath(const std::string_view path) {
			return path.starts_with("content/") || path.starts_with("projects/");
		}

		[[nodiscard]] std::string ResolvePresetPath(std::string path) {
			path = StrUtil::NormalizePath(std::move(path));
			if (path.empty()) {
				return {};
			}

			std::filesystem::path fsPath(path);
			if (
				!fsPath.is_absolute() && !IsCurrentDirectoryRelativePath(path) &&
				IsEngineRootRelativePath(path)
			) {
				path = "./" + path;
			}

			return path;
		}

		Render::WORLD_PARTICLE_SHAPE ConvertShape(const VertexDataType shape) {
			switch (shape) {
				case VertexDataType::Plane: return Render::WORLD_PARTICLE_SHAPE::PLANE;
				case VertexDataType::Ring: return Render::WORLD_PARTICLE_SHAPE::RING;
				case VertexDataType::Cylinder: return Render::WORLD_PARTICLE_SHAPE::CYLINDER;
				default: return Render::WORLD_PARTICLE_SHAPE::PLANE;
			}
		}

		Render::WORLD_PARTICLE_BLEND_MODE ConvertBlendMode(const BlendMode blendMode) {
			switch (blendMode) {
				case kBlendModeNone: return Render::WORLD_PARTICLE_BLEND_MODE::NONE;
				case kBlendModeNormal: return Render::WORLD_PARTICLE_BLEND_MODE::NORMAL;
				case kBlendModeAdd: return Render::WORLD_PARTICLE_BLEND_MODE::ADD;
				case kBlendModeSubtract: return Render::WORLD_PARTICLE_BLEND_MODE::SUBTRACT;
				case kBlendModeMultiply: return Render::WORLD_PARTICLE_BLEND_MODE::MULTIPLY;
				case kBlendModeScreen: return Render::WORLD_PARTICLE_BLEND_MODE::SCREEN;
				default: return Render::WORLD_PARTICLE_BLEND_MODE::NORMAL;
			}
		}
	}

	std::string_view ParticleEmitterComponent::GetStableName() const {
		return "engine.ParticleEmitter";
	}

	std::string_view ParticleEmitterComponent::GetComponentName() const {
		return "ParticleEmitter";
	}

	void ParticleEmitterComponent::OnTick(const float deltaTime) {
		if (!EnsurePresetLoaded()) {
			return;
		}

		auto* owner = GetOwner();
		auto* transform = owner ? owner->GetComponent<TransformComponent>() : nullptr;
		if (transform) {
			mEmitter.SetTransform(transform->RenderWorldMat());
		}

		mEmitter.Update(std::max(0.0f, deltaTime * mTimeScale));

		DrawEmitterShapeDebug();
	}

	void ParticleEmitterComponent::DrawEmitterShapeDebug() const {
		if (!mDrawEmitterShape || mPreset == nullptr) {
			return;
		}

		const EmitterSpawnModule& spawn = mPreset->emitterSpawn;
		// ランダム位置が無効なら発生範囲は 1 点なので形状は描かない。
		if (!spawn.useRandomPosition) {
			return;
		}

		World* world = GetWorld();
		if (world == nullptr) {
			return;
		}

		// 発生形状はエミッタのワールド座標を中心に置く。
		// ※ GetTranslate() は非 const のためローカルコピーを介する。
		Mat4 emitterTransform = mEmitter.GetTransform();
		const Vec3 center = emitterTransform.GetTranslate();

		// ローカル空間モードのときは発生形状もエミッタの回転に追従する。
		const Quaternion orientation = mEmitter.IsLocalSpace()
			                               ? emitterTransform.ToQuaternion()
			                               : Quaternion::identity;

		WorldDebugDraw& debugDraw = world->GetDebugDraw();
		const Vec4 color{ 0.2f, 1.0f, 0.3f, 1.0f }; // 緑

		// エミッタ基準の各軸（ローカル空間なら orientation により回転済み）。
		const Vec3 axisX = orientation * Vec3::right;
		const Vec3 axisY = orientation * Vec3::up;
		const Vec3 axisZ = orientation * Vec3::forward;

		// DrawCircle は単位回転だと XY 平面に描かれる。
		// エミッタ形状は Y 軸基準なので、円を XZ 平面へ倒す 90° 補正をかける。
		constexpr float  kHalfPi = 1.57079633f;
		const Quaternion circleToXZ =
			orientation * Quaternion::Euler(kHalfPi, 0.0f, 0.0f);

		// XZ 円周上の点（angleRad）を求めるヘルパー。
		const auto rimPoint = [&](const Vec3& discCenter, float radius,
		                          float angleRad) {
			return discCenter
				+ axisX * (std::cos(angleRad) * radius)
				+ axisZ * (std::sin(angleRad) * radius);
		};

		constexpr uint32_t kSegments = 24;

		switch (spawn.emitShape) {
		case EmitShapeType::Sphere:
			debugDraw.DrawSphere(center, orientation, spawn.sphereRadius, color);
			break;

		case EmitShapeType::Cone: {
			// 頂点 = center、底面 = center + axisY * height。
			const Vec3 baseCenter = center + axisY * spawn.coneHeight;
			debugDraw.DrawCircle(
				baseCenter, circleToXZ, spawn.coneRadius, color, kSegments);
			for (int i = 0; i < 4; ++i) {
				const float a = static_cast<float>(i) * kHalfPi;
				debugDraw.DrawLine(
					center, rimPoint(baseCenter, spawn.coneRadius, a), color);
			}
			break;
		}

		case EmitShapeType::Cylinder: {
			const Vec3 half         = axisY * (spawn.cylinderHeight * 0.5f);
			const Vec3 topCenter    = center + half;
			const Vec3 bottomCenter = center - half;
			debugDraw.DrawCircle(
				topCenter, circleToXZ, spawn.cylinderRadius, color, kSegments);
			debugDraw.DrawCircle(
				bottomCenter, circleToXZ, spawn.cylinderRadius, color, kSegments);
			for (int i = 0; i < 4; ++i) {
				const float a = static_cast<float>(i) * kHalfPi;
				debugDraw.DrawLine(
					rimPoint(topCenter, spawn.cylinderRadius, a),
					rimPoint(bottomCenter, spawn.cylinderRadius, a),
					color);
			}
			break;
		}

		case EmitShapeType::Circle:
			debugDraw.DrawCircle(
				center, circleToXZ, spawn.circleRadius, color, kSegments);
			break;

		case EmitShapeType::Box:
		default:
			// boxHalfSize は各軸の半径なので、DrawBox には全体サイズ(2倍)を渡す。
			debugDraw.DrawBox(
				center,
				orientation,
				Vec3{
					spawn.boxHalfSize.x * 2.0f,
					spawn.boxHalfSize.y * 2.0f,
					spawn.boxHalfSize.z * 2.0f,
				},
				color
			);
			break;
		}
	}

#ifdef _DEBUG
	void ParticleEmitterComponent::DrawInspectorImGui() {
		std::array<char, 256> presetBuffer = {};
		const size_t copyLength = std::min(mPresetName.size(), presetBuffer.size() - 1);
		std::copy_n(mPresetName.data(), copyLength, presetBuffer.data());
		if (ImGui::InputText("Preset Name", presetBuffer.data(), presetBuffer.size())) {
			mPresetName = presetBuffer.data();
			mPreset = nullptr;
			mHasWarnedLoadFailure = false;
			mLoadedPresetPath.clear();
			mPresetAssetId = kInvalidAssetID;
			mPresetAssetVersion = 0;
		}

		std::array<char, 512> presetPathBuffer = {};
		const size_t presetPathLength = std::min(
			mPresetPath.size(),
			presetPathBuffer.size() - 1
		);
		std::copy_n(
			mPresetPath.data(),
			presetPathLength,
			presetPathBuffer.data()
		);
		if (
			ImGui::InputText(
				"Preset Path",
				presetPathBuffer.data(),
				presetPathBuffer.size()
			)
		) {
			mPresetPath = presetPathBuffer.data();
			mPreset = nullptr;
			mHasWarnedLoadFailure = false;
			mLoadedPresetPath.clear();
			mPresetAssetId = kInvalidAssetID;
			mPresetAssetVersion = 0;
		}

		ImGui::Checkbox("Auto Play", &mAutoPlay);
		ImGui::Checkbox("Depth Test", &mDepthTest);
		ImGui::Checkbox("Draw Emitter Shape", &mDrawEmitterShape);
		ImGui::DragInt("Sort Key Bias", &mSortKeyBias, 1.0f, -100000, 100000);
		ImGui::DragFloat("Time Scale", &mTimeScale, 0.01f, 0.0f, 100.0f, "%.2f");
		if (ImGui::DragFloat("Start Delay", &mStartDelay, 0.01f, -1.0f, 100.0f, "%.2f")) {
			mEmitter.SetStartDelayOverride(mStartDelay);
		}
		ImGui::Text("Live Particles: %zu", mEmitter.GetParticles().size());
	}
#endif

	void ParticleEmitterComponent::Deserialize(const JsonReader& reader) {
		mPresetName = reader["presetName"].GetString(mPresetName);
		mPresetPath = reader["presetPath"].GetString(mPresetPath);
		mAutoPlay = reader["autoPlay"].GetBool(mAutoPlay);
		mDepthTest = reader["depthTest"].GetBool(mDepthTest);
		mDrawEmitterShape = reader["drawEmitterShape"].GetBool(mDrawEmitterShape);
		mSortKeyBias = reader["sortKeyBias"].GetInt(mSortKeyBias);
		mTimeScale = reader["timeScale"].GetFloat(mTimeScale);
		mStartDelay = reader["startDelay"].GetFloat(mStartDelay);
		mPreset = nullptr;
		mHasWarnedLoadFailure = false;
		mLoadedPresetPath.clear();
		mPresetAssetId = kInvalidAssetID;
		mPresetAssetVersion = 0;
	}

	void ParticleEmitterComponent::Serialize(JsonWriter& writer) const {
		writer.Key("presetName");
		writer.Write(mPresetName);
		writer.Key("presetPath");
		writer.Write(mPresetPath);
		writer.Key("autoPlay");
		writer.Write(mAutoPlay);
		writer.Key("depthTest");
		writer.Write(mDepthTest);
		writer.Key("drawEmitterShape");
		writer.Write(mDrawEmitterShape);
		writer.Key("sortKeyBias");
		writer.Write(mSortKeyBias);
		writer.Key("timeScale");
		writer.Write(mTimeScale);
		writer.Key("startDelay");
		writer.Write(mStartDelay);
	}

	void ParticleEmitterComponent::GatherWorldParticles(
		std::vector<Render::WorldParticleInput>& outParticles
	) {
		if (!EnsurePresetLoaded()) {
			return;
		}

		auto* owner = GetOwner();
		auto* transform = owner ? owner->GetComponent<TransformComponent>() : nullptr;
		if (!transform) {
			return;
		}

		const AssetID textureAssetId = ResolveTextureAssetId();
		const Mat4 ownerWorld = transform->RenderWorldMat();
		const bool localSpace = mEmitter.IsLocalSpace();
		const bool useBillboard = mEmitter.IsBillboard();
		const Render::WORLD_PARTICLE_SHAPE renderShape = ResolveRenderShape();
		const Render::WORLD_PARTICLE_BLEND_MODE renderBlendMode = ResolveRenderBlendMode();

		for (const Particle& particle : mEmitter.GetParticles()) {
			if (!particle.active) {
				continue;
			}

			Render::WorldParticleInput input = {};
			input.texture.source = Render::SPRITE_TEXTURE_SOURCE::ASSET;
			input.texture.textureAssetId = textureAssetId;
			input.color = particle.color;
			input.scale = Vec3(
				std::max(1e-4f, particle.scale.x),
				std::max(1e-4f, particle.scale.y),
				std::max(1e-4f, particle.scale.z)
			);
			input.rotation = particle.rotation;
			input.sortKey = mSortKeyBias;
			input.depthTest = mDepthTest;
			input.useBillboard = useBillboard;
			input.flipY = mEmitter.IsFlipY();
			input.shape = renderShape;
			input.blendMode = renderBlendMode;

			const Mat4 particleLocal = Mat4::Affine(
				Vec3::one,
				particle.rotation,
				particle.position
			);
			Mat4 particleWorld = localSpace ? particleLocal * ownerWorld : particleLocal;
			input.worldPosition = particleWorld.GetTranslate();
			input.worldRight = particleWorld.GetRight();
			input.worldUp = particleWorld.GetUp();
			input.worldForward = particleWorld.GetForward();

			outParticles.emplace_back(std::move(input));

			// --- トレイル: 履歴点ごとにビルボード板ポリを並べる ---
			if (mPreset->trail.enabled && particle.trailPoints.size() >= 2) {
				const TrailModuleSettings& trail = mPreset->trail;
				const std::vector<Vec3>& points = particle.trailPoints;
				const int n = static_cast<int>(points.size());

				for (int i = 0; i < n; ++i) {
					// t: 0=末端(古い履歴) .. 1=先端(新しい履歴)
					const float t =
						static_cast<float>(i) / static_cast<float>(n - 1);

					const float width = std::max(
						1e-4f,
						trail.widthTail + (trail.widthHead - trail.widthTail) * t
					);

					Render::WorldParticleInput trailInput = {};
					trailInput.texture.source =
						Render::SPRITE_TEXTURE_SOURCE::ASSET;
					trailInput.texture.textureAssetId = textureAssetId;
					trailInput.color = Vec4{
						trail.colorTail.x +
							(trail.colorHead.x - trail.colorTail.x) * t,
						trail.colorTail.y +
							(trail.colorHead.y - trail.colorTail.y) * t,
						trail.colorTail.z +
							(trail.colorHead.z - trail.colorTail.z) * t,
						trail.colorTail.w +
							(trail.colorHead.w - trail.colorTail.w) * t,
					};
					trailInput.scale = Vec3(width, width, width);
					trailInput.rotation = Vec3::zero;
					trailInput.sortKey = mSortKeyBias;
					trailInput.depthTest = mDepthTest;
					trailInput.useBillboard = true; // ビーズは常にカメラ目線
					trailInput.flipY = mEmitter.IsFlipY();
					trailInput.shape = renderShape;
					trailInput.blendMode = renderBlendMode;

					const Mat4 trailLocal = Mat4::Affine(
						Vec3::one, Vec3::zero, points[i]
					);
					Mat4 trailWorld =
						localSpace ? trailLocal * ownerWorld : trailLocal;
					trailInput.worldPosition = trailWorld.GetTranslate();
					trailInput.worldRight = trailWorld.GetRight();
					trailInput.worldUp = trailWorld.GetUp();
					trailInput.worldForward = trailWorld.GetForward();

					outParticles.emplace_back(std::move(trailInput));
				}
			}
		}
	}

	bool ParticleEmitterComponent::EnsurePresetLoaded() {
		if (mPresetName.empty() && mPresetPath.empty()) {
			return false;
		}

		const std::string resolvedPresetPath = ResolvePresetFilePath();
		if (resolvedPresetPath.empty()) {
			return false;
		}

		if (!EnsurePresetAssetTracked(resolvedPresetPath)) {
			return LoadPresetFromFile(resolvedPresetPath);
		}

		if (mPreset == nullptr || mLoadedPresetPath != resolvedPresetPath) {
			return LoadPresetFromFile(resolvedPresetPath);
		}

		AssetManager* assetManager = GetAssetManager();
		if (assetManager != nullptr && mPresetAssetId != kInvalidAssetID) {
			const AssetMetaData& meta = assetManager->Meta(mPresetAssetId);
			if (meta.version != mPresetAssetVersion) {
				return LoadPresetFromFile(resolvedPresetPath);
			}
		}

		if (!mPresetName.empty() && mPreset->name != mPresetName) {
			return LoadPresetFromFile(resolvedPresetPath);
		}

		return true;
	}

	std::string ParticleEmitterComponent::ResolvePresetFilePath() const {
		if (!mPresetPath.empty()) {
			std::filesystem::path presetPath = ResolvePresetPath(mPresetPath);
			if (presetPath.extension() != ".json") {
				presetPath.replace_extension(".json");
			}
			return presetPath.string();
		}

		if (mPresetName.empty()) {
			return {};
		}

		std::filesystem::path presetPath =
			std::filesystem::path(std::string(kPresetDirectory)) /
			(mPresetName + ".json");
		return ResolvePresetPath(presetPath.string());
	}

	bool ParticleEmitterComponent::EnsurePresetAssetTracked(
		const std::string& resolvedPresetPath
	) {
		AssetManager* assetManager = GetAssetManager();
		if (assetManager == nullptr) {
			return false;
		}

		if (
			mPresetAssetId != kInvalidAssetID && mLoadedPresetPath == resolvedPresetPath
		) {
			return true;
		}

		const AssetID assetId = assetManager->LoadFromFile(
			resolvedPresetPath,
			ASSET_TYPE::PARTICLE_PRESET
		);
		if (assetId == kInvalidAssetID) {
			return false;
		}

		mPresetAssetId = assetId;
		const AssetMetaData& meta = assetManager->Meta(assetId);
		mPresetAssetVersion = meta.version;
		return true;
	}

	bool ParticleEmitterComponent::LoadPresetFromFile(
		const std::string& resolvedPresetPath
	) {
		std::filesystem::path presetPath(resolvedPresetPath);
		ParticlePreset loadedPreset = {};
		if (
			!mPresetLibrary.LoadFromJson(
				presetPath.stem().string(),
				loadedPreset,
				presetPath.parent_path().string()
			)
		) {
			if (!mHasWarnedLoadFailure) {
				Warning(
					kLogChannel,
					"Failed to load particle preset from path '{}'",
					presetPath.string()
				);
				mHasWarnedLoadFailure = true;
			}
			return false;
		}

		const std::string resolvedName = !loadedPreset.name.empty()
			                                 ? loadedPreset.name
			                                 : presetPath.stem().string();
		if (loadedPreset.name.empty()) {
			loadedPreset.name = resolvedName;
			mPresetLibrary.Add(loadedPreset);
		}

		ParticlePreset* preset = mPresetLibrary.Find(resolvedName);
		if (preset == nullptr) {
			return false;
		}

		mPreset = preset;
		mHasWarnedLoadFailure = false;
		mLoadedPresetPath = resolvedPresetPath;
		if (!mPresetPath.empty() && mPresetName.empty()) {
			mPresetName = resolvedName;
		}
		AssetManager* assetManager = GetAssetManager();
		if (assetManager != nullptr && mPresetAssetId != kInvalidAssetID) {
			mPresetAssetVersion = assetManager->Meta(mPresetAssetId).version;
		}
		ResetEmitterFromPreset();
		return true;
	}

	void ParticleEmitterComponent::ResetEmitterFromPreset() {
		if (!mPreset) {
			return;
		}

		auto* owner = GetOwner();
		auto* transform = owner ? owner->GetComponent<TransformComponent>() : nullptr;
		const Mat4 emitterTransform = transform ? transform->RenderWorldMat() : Mat4::identity;

		mEmitter.SetStartDelayOverride(mStartDelay);
		mEmitter.Initialize(mPreset);
		mEmitter.SetTransform(emitterTransform);
		if (mAutoPlay) {
			mEmitter.Play();
		} else {
			mEmitter.Stop();
		}

		mCachedTexturePath.clear();
		mCachedTextureAssetId = kInvalidAssetID;
	}

	AssetID ParticleEmitterComponent::ResolveTextureAssetId() {
		if (!mPreset) {
			return kInvalidAssetID;
		}

		AssetManager* assetManager = GetAssetManager();
		if (!assetManager) {
			return kInvalidAssetID;
		}

		const std::string& texturePath = mPreset->emitterSettings.textureFilePath;
		if (texturePath.empty()) {
			mCachedTexturePath.clear();
			mCachedTextureAssetId = kInvalidAssetID;
			return kInvalidAssetID;
		}

		if (texturePath != mCachedTexturePath || mCachedTextureAssetId == kInvalidAssetID) {
			mCachedTextureAssetId = assetManager->LoadFromFile(texturePath, ASSET_TYPE::TEXTURE);
			mCachedTexturePath = texturePath;
		}

		return mCachedTextureAssetId;
	}

	Render::WORLD_PARTICLE_SHAPE ParticleEmitterComponent::ResolveRenderShape() const {
		if (!mPreset) {
			return Render::WORLD_PARTICLE_SHAPE::PLANE;
		}
		return ConvertShape(mPreset->emitterSettings.vertexType);
	}

	Render::WORLD_PARTICLE_BLEND_MODE ParticleEmitterComponent::ResolveRenderBlendMode() const {
		if (!mPreset) {
			return Render::WORLD_PARTICLE_BLEND_MODE::NORMAL;
		}
		return ConvertBlendMode(mPreset->emitterSettings.blendMode);
	}

	REGISTER_COMPONENT(ParticleEmitterComponent);
}
