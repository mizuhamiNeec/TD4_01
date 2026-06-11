#pragma once
#ifdef _DEBUG

#include <cstdint>
#include <string>
#include <vector>

#include <core/assets/AssetID.h>
#include <core/assets/types/SequenceAssetData.h>

#include "../IEditorTool.h"
#include "SequenceEditorTypes.h"

namespace Unnamed {
	class AssetManager;
	class SequenceEditorController;
	class World;

	/// @brief Sequence Editorタイムライン上で扱うキーフレーム種別です。
	enum class SequenceTimelineKeyKind : uint8_t {
		Float,
		Bool,
		CameraCut,
		Event,
	};

	/// @brief Sequence Editorタイムライン上のキーフレーム選択です。
	struct SequenceTimelineKeySelection final {
		int32_t trackIndex = -1;
		int32_t sectionIndex = -1;
		SEQUENCE_EDITOR_FLOAT_CHANNEL floatChannel =
			SEQUENCE_EDITOR_FLOAT_CHANNEL::NONE;
		SequenceTimelineKeyKind kind = SequenceTimelineKeyKind::Float;
		uint64_t keyId = 0;
	};

	/// @brief Sequence Editorタイムラインのキードラッグ開始状態です。
	struct SequenceTimelineKeyDragStart final {
		SequenceTimelineKeySelection selection = {};
		int64_t frame = 0;
	};

	/// @brief シーケンスアセットを編集するためのエディタです。
	class SequenceEditorTool : public IEditorTool {
	public:
		/// @brief コンストラクタです。
		SequenceEditorTool();

		/// @brief デストラクタです。
		~SequenceEditorTool() override;

		[[nodiscard]] std::string_view GetToolId() const override;
		[[nodiscard]] std::string_view GetDisplayName() const override;
		/// @brief Sequence Editor共有コントローラを設定します。
		void SetController(SequenceEditorController* controller);
		/// @brief プレビュー適用先RuntimeWorldを設定します。
		void SetRuntimeWorld(World* world);
		void Initialize(const EditorToolServices& services) override;
		void Shutdown() override;
		void Tick(const EditorToolFrameContext& frameContext) override;
		void BuildUi(const EditorToolFrameContext& frameContext) override;
		void CollectRenderViews(Render::RenderFrameInputs& inputs) override;
		void EnumerateViewKeys(
			std::vector<std::string>& outViewKeys
		) const override;
		void SetViewOutput(
			std::string_view viewKey,
			const Render::SceneOutputView& output,
			Vec2 size
		) override;
		[[nodiscard]] bool IsOpen() const override;
		void SetOpen(bool open) override;

	private:
		// 編集中のシーケンスアセットID。編集中のシーケンスがない場合はkInvalidAssetID。
		AssetID mEditingSequenceId = kInvalidAssetID;
		std::string mSequenceAssetPath;
		AssetManager* mAssetManager = nullptr;
		World* mRuntimeWorld = nullptr;

		SequenceEditorController* mController = nullptr;
		std::vector<SequenceTimelineKeySelection> mSelectedKeys = {};
		std::vector<SequenceTimelineKeyDragStart> mKeyDragStart = {};

		float mPixelsPerFrame = 12.0f;
		float mTimelineScrollY = 0.0f;
		float mKeyDragFrameDelta = 0.0f;
		bool mTransformPositionExpanded = true;
		bool mTransformRotationExpanded = true;
		bool mTransformScaleExpanded = true;
		bool mDraggingKeys = false;
		bool mDraggingPlayhead = false;
		bool mBoxSelecting = false;
		Vec2 mBoxSelectStart = Vec2::zero;
		Vec2 mBoxSelectEnd = Vec2::zero;

		int64_t mContextFrame = 0;
		int32_t mContextTrackIndex = -1;
		int32_t mContextSectionIndex = -1;
		SEQUENCE_EDITOR_FLOAT_CHANNEL mContextFloatChannel =
			SEQUENCE_EDITOR_FLOAT_CHANNEL::NONE;
		SequenceTimelineKeyKind mContextKeyKind = SequenceTimelineKeyKind::Float;

		SEQUENCE_TRACK_TYPE mAddTrackType = SEQUENCE_TRACK_TYPE::TRANSFORM;
		std::string mAddTrackName;
		uint64_t mAddTrackEntityGuid = 0;

		bool mOpen = true;
	};
}
#endif
