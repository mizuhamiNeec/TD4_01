#pragma once
#ifdef _DEBUG

namespace Unnamed {
	class SequenceEditorController;

	/// @brief 選択中チャンネルのカーブ編集パネルです。
	class SequenceCurvePanel final {
	public:
		/// @brief カーブパネルを描画します。
		void Draw(SequenceEditorController& controller);
	};
}

#endif
