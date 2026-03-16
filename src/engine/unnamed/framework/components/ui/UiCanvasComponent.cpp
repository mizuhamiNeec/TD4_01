#include "UiCanvasComponent.h"

#include <array>
#include <algorithm>
#include <cstring>

#include <imgui.h>

#include "core/ComponentRegistry.h"
#include "core/assets/AssetManager.h"
#include "core/assets/types/UiDocumentAssetData.h"
#include "core/json/JsonReader.h"
#include "core/json/JsonWriter.h"
#include "core/string/StrUtil.h"

#include "engine/gui/UiDocument.h"
#include "engine/gui/UiRoot.h"
#include "engine/gui/UiWidget.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"

namespace Unnamed {
	namespace {
		static constexpr std::string_view kChannel = "UiCanvasComponent";

		std::string ToModeString(const UI_CANVAS_SPACE_MODE mode) {
			switch (mode) {
				case UI_CANVAS_SPACE_MODE::SCREEN: return "Screen";
				case UI_CANVAS_SPACE_MODE::WORLD_BILLBOARD:
					return "WorldBillboard";
				case UI_CANVAS_SPACE_MODE::WORLD_PLANE: return "WorldPlane";
				default: return "Screen";
			}
		}

		UI_CANVAS_SPACE_MODE ParseMode(const std::string_view value) {
			if (value == "WorldBillboard") {
				return UI_CANVAS_SPACE_MODE::WORLD_BILLBOARD;
			}
			if (value == "WorldPlane") {
				return UI_CANVAS_SPACE_MODE::WORLD_PLANE;
			}
			return UI_CANVAS_SPACE_MODE::SCREEN;
		}
	}

	UiCanvasComponent::UiCanvasComponent() = default;
	UiCanvasComponent::~UiCanvasComponent() = default;

	void UiCanvasComponent::Deserialize(const JsonReader& reader) {
		if (reader.Has("uiAssetPath")) {
			SetUiAssetPath(reader["uiAssetPath"].GetString());
		}
		if (reader.Has("spaceMode")) {
			SetSpaceMode(ParseMode(reader["spaceMode"].GetString()));
		}
		if (reader.Has("pixelSize")) {
			const JsonReader pixel = reader["pixelSize"].GetArray();
			if (pixel.Size() >= 2) {
				SetPixelSize(Vec2(pixel[0].GetFloat(), pixel[1].GetFloat()));
			}
		}
		if (reader.Has("worldSize")) {
			const JsonReader world = reader["worldSize"].GetArray();
			if (world.Size() >= 2) {
				SetWorldSize(Vec2(world[0].GetFloat(), world[1].GetFloat()));
			}
		}
		if (reader.Has("sortKey")) {
			SetSortKey(reader["sortKey"].GetInt());
		}
		if (reader.Has("receiveInput")) {
			SetReceiveInput(reader["receiveInput"].GetBool());
		}
	}

	void UiCanvasComponent::Serialize(JsonWriter& writer) const {
		writer.Key("uiAssetPath");
		writer.Write(mUiAssetPath);

		writer.Key("spaceMode");
		writer.Write(ToModeString(mSpaceMode));

		writer.Key("pixelSize");
		writer.BeginArray();
		writer.Write(mPixelSize.x);
		writer.Write(mPixelSize.y);
		writer.EndArray();

		writer.Key("worldSize");
		writer.BeginArray();
		writer.Write(mWorldSize.x);
		writer.Write(mWorldSize.y);
		writer.EndArray();

		writer.Key("sortKey");
		writer.Write(mSortKey);

		writer.Key("receiveInput");
		writer.Write(mReceiveInput);
	}

#ifdef _DEBUG
	void UiCanvasComponent::DrawInspectorImGui() {
		std::array<char, 512> pathBuffer = {};
		memcpy(
			pathBuffer.data(),
			mUiAssetPath.c_str(),
			std::min(mUiAssetPath.size(), pathBuffer.size() - 1)
		);
		if (ImGui::InputText("UI Asset Path", pathBuffer.data(), pathBuffer.size())) {
			SetUiAssetPath(pathBuffer.data());
		}

		constexpr const char* kModes[] = {"Screen", "WorldBillboard", "WorldPlane"};
		int mode = static_cast<int>(mSpaceMode);
		if (ImGui::Combo("Space Mode", &mode, kModes, IM_ARRAYSIZE(kModes))) {
			SetSpaceMode(static_cast<UI_CANVAS_SPACE_MODE>(mode));
		}

		float pixel[2] = {mPixelSize.x, mPixelSize.y};
		if (ImGui::DragFloat2("Pixel Size", pixel, 1.0f, 1.0f, 8192.0f)) {
			SetPixelSize(Vec2(pixel[0], pixel[1]));
		}

		float world[2] = {mWorldSize.x, mWorldSize.y};
		if (ImGui::DragFloat2("World Size", world, 0.01f, 0.01f, 1000.0f)) {
			SetWorldSize(Vec2(world[0], world[1]));
		}

		ImGui::DragInt("Sort Key", &mSortKey, 1.0f);
		ImGui::Checkbox("Receive Input", &mReceiveInput);
	}
#endif

	void UiCanvasComponent::SetUiAssetPath(const std::string& path) {
		const std::string normalized = path.empty() ?
			                               std::string() :
			                               StrUtil::NormalizePath(path);
		if (mUiAssetPath == normalized) {
			return;
		}
		mUiAssetPath = normalized;
		mUiAssetId   = kInvalidAssetID;
		InvalidateRuntime();
	}

	const std::string& UiCanvasComponent::GetUiAssetPath() const {
		return mUiAssetPath;
	}

	void UiCanvasComponent::SetSpaceMode(const UI_CANVAS_SPACE_MODE mode) {
		mSpaceMode = mode;
	}

	UI_CANVAS_SPACE_MODE UiCanvasComponent::GetSpaceMode() const {
		return mSpaceMode;
	}

	void UiCanvasComponent::SetPixelSize(const Vec2& size) {
		mPixelSize = Vec2(std::max(1.0f, size.x), std::max(1.0f, size.y));
	}

	Vec2 UiCanvasComponent::GetPixelSize() const {
		return mPixelSize;
	}

	void UiCanvasComponent::SetWorldSize(const Vec2& size) {
		mWorldSize = Vec2(std::max(0.01f, size.x), std::max(0.01f, size.y));
	}

	Vec2 UiCanvasComponent::GetWorldSize() const {
		return mWorldSize;
	}

	void UiCanvasComponent::SetSortKey(const int32_t sortKey) {
		mSortKey = sortKey;
	}

	int32_t UiCanvasComponent::GetSortKey() const {
		return mSortKey;
	}

	void UiCanvasComponent::SetReceiveInput(const bool receiveInput) {
		mReceiveInput = receiveInput;
	}

	bool UiCanvasComponent::GetReceiveInput() const {
		return mReceiveInput;
	}

	bool UiCanvasComponent::EnsureRuntimeLoaded() {
		if (mUiAssetPath.empty()) {
			if (!mLoggedLoadFailure) {
				Error(kChannel, "UI asset path is empty.");
				mLoggedLoadFailure = true;
			}
			return false;
		}

		auto* assetManager = ServiceLocator::Get<AssetManager>();
		if (!assetManager) {
			if (!mLoggedLoadFailure) {
				Error(kChannel, "AssetManager is not available.");
				mLoggedLoadFailure = true;
			}
			return false;
		}

		if (mUiAssetId == kInvalidAssetID || mLoadedAssetPath != mUiAssetPath) {
			mUiAssetId = assetManager->LoadFromFile(
				mUiAssetPath, ASSET_TYPE::UI_DOCUMENT
			);
			mLoadedAssetVersion = 0;
		}
		if (mUiAssetId == kInvalidAssetID) {
			if (!mLoggedLoadFailure) {
				Error(kChannel, "Failed to resolve UI asset '{}'.", mUiAssetPath);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		const auto& meta = assetManager->Meta(mUiAssetId);
		if (
			mRuntimeRoot &&
			mLoadedAssetPath == mUiAssetPath &&
			mLoadedAssetVersion == meta.version
		) {
			return true;
		}

		const auto* assetData = assetManager->Get<UiDocumentAssetData>(mUiAssetId);
		if (!assetData) {
			if (!mLoggedLoadFailure) {
				Error(kChannel, "UI asset '{}' payload is invalid.", mUiAssetPath);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		auto document = Gui::UiDocument::LoadFromJson(
			JsonReader(assetData->rootJson),
			mUiAssetPath
		);
		if (!document) {
			if (!mLoggedLoadFailure) {
				Error(kChannel, "Failed to load UI asset '{}'.", mUiAssetPath);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		auto rootWidget = document->TakeRootWidget();
		if (!rootWidget) {
			if (!mLoggedLoadFailure) {
				Error(kChannel, "UI asset '{}' has no root widget.", mUiAssetPath);
				mLoggedLoadFailure = true;
			}
			return false;
		}

		mRuntimeRoot = std::make_unique<Gui::UiRoot>();
		mRuntimeRoot->AddChild(std::move(rootWidget));
		mLoadedAssetPath   = mUiAssetPath;
		mLoadedAssetVersion = meta.version;
		mLoggedLoadFailure = false;
		return true;
	}

	void UiCanvasComponent::TickRuntime(const float deltaTime) const {
		if (mRuntimeRoot) {
			mRuntimeRoot->Tick(deltaTime);
		}
	}

	Gui::UiRoot* UiCanvasComponent::GetRuntimeRoot() const {
		return mRuntimeRoot.get();
	}

	void UiCanvasComponent::OnDetached() {
		InvalidateRuntime();
	}

	void UiCanvasComponent::InvalidateRuntime() {
		mRuntimeRoot.reset();
		mLoadedAssetPath.clear();
		mLoadedAssetVersion = 0;
	}

	REGISTER_COMPONENT(UiCanvasComponent);
}
