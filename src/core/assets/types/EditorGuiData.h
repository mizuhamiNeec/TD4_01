#include <string>

namespace Unnamed {
	struct EditorGuiData {
		std::string sourcePath; // ソースファイルのパス
		std::string source;     // ソースコードの内容
		std::string lastError;  // 最後のエラーメッセージ...?
		bool        hasError;   // スクリプトにエラーがあるか?
	};
}
