#pragma once
#ifdef _DEBUG

namespace Unnamed {
	class SequenceEditorController;

	/// @brief Sequence Timelineドックを描画するパネルです。
	class SequenceTimelinePanel final {
	public:
		/// @brief Timelineパネルを描画します。
		void Draw(SequenceEditorController& controller);
	};
}

#endif
