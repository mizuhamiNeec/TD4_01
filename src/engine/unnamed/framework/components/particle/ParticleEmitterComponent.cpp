#include "ParticleEmitterComponent.h"

#include <algorithm>
#include <array>
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
		ImGui::DragInt("Sort Key Bias", &mSortKeyBias, 1.0f, -100000, 100000);
		ImGui::DragFloat("Time Scale", &mTimeScale, 0.01f, 0.0f, 100.0f, "%.2f");
		ImGui::Text("Live Particles: %zu", mEmitter.GetParticles().size());
	}
#endif

	void ParticleEmitterComponent::Deserialize(const JsonReader& reader) {
		mPresetName = reader["presetName"].GetString(mPresetName);
		mPresetPath = reader["presetPath"].GetString(mPresetPath);
		mAutoPlay = reader["autoPlay"].GetBool(mAutoPlay);
		mDepthTest = reader["depthTest"].GetBool(mDepthTest);
		mSortKeyBias = reader["sortKeyBias"].GetInt(mSortKeyBias);
		mTimeScale = reader["timeScale"].GetFloat(mTimeScale);
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
		writer.Key("sortKeyBias");
		writer.Write(mSortKeyBias);
		writer.Key("timeScale");
		writer.Write(mTimeScale);
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
