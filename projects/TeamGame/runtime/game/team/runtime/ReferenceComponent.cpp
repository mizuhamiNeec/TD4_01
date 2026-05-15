/*
================================================================================
                        ReferenceComponent.cpp
================================================================================

　本ファイルは、Unnamed Game Engine でコンポーネントを実装する際の
　テンプレート・リファレンスです。

================================================================================
*/

#include "ReferenceComponent.h"

// -----------------------------------------------------------------------
// システムインクルード
// -----------------------------------------------------------------------

#include "engine/unnamed/framework/entity/Entity.h"
#include "engine/unnamed/subsystem/interface/ServiceLocator.h"
#include "./core/ComponentRegistry.h"

// デバッグビルド時のみ ImGui をインクルード
#ifdef _DEBUG
#include "imgui.h"
#endif

namespace MyGame {

	// =========================================================================
	// ライフサイクル メソッドの実装
	// =========================================================================

	void ReferenceComponent::OnAttached() {
		// -----------------------------------------------------------------------
		// コンポーネント初期化処理
		// -----------------------------------------------------------------------
		// 
		// 【実行タイミング】
		//   - コンポーネントがエンティティにアタッチされるときに、1回だけ呼び出される
		// 
		// 【典型的な処理】
		//   1. メンバ変数の初期化
		//   2. 他のコンポーネントへのポインタ取得・キャッシュ
		//   3. リソースの確保
		//   4. InputSystem のセットアップ
		//   5. イベントリスナーの登録
		// 
		// 【例】
		//   auto* inputSystem = ServiceLocator::Get<Unnamed::InputSystem>();
		//   _anotherComponent = GetOwner()->GetComponent<AnotherComponent>();
		//

		// NOTE: 初期化が必要な場合、ここに処理を記述
		_sampleValue = 0.0f;
		_sampleString = "default";
	}

	void ReferenceComponent::OnTick(float deltaTime) {
		// -----------------------------------------------------------------------
		// 毎フレーム更新処理
		// -----------------------------------------------------------------------
		// 
		// 【実行タイミング】
		//   - ゲームの各フレームごとに呼び出される
		//   - deltaTime には前フレームからの経過時間（秒）が渡される
		// 
		// 【典型的な処理】
		//   1. ゲームロジックの更新
		//   2. 入力の処理
		//   3. 物理計算
		//   4. アニメーション更新
		//   5. 状態管理
		// 
		// 【パフォーマンスの注意点】
		//   - 毎フレーム呼び出されるため、重い処理は避ける
		//   - メモリの確保・解放はしない（GC の負担になる）
		//   - 大きなデータ構造のコピーはしない
		//   - 重い計算はキャッシュする
		// 
		// 【例】
		//   // 値を時間に応じて更新
		//   _sampleValue += deltaTime;
		// 
		//   // 入力を処理
		//   if (inputSystem->IsPressed("Fire")) {
		//       Fire();
		//   }
		//

		// NOTE: 更新処理が必要な場合、ここに記述
		// 以下は使用例です。実装時は削除または変更してください。

		// 例: 毎フレーム値を増加
		_sampleValue += deltaTime;

		// 例: 値が1.0を超えたらリセット
		if (_sampleValue > 1.0f) {
			_sampleValue = 0.0f;
		}
	}

	void ReferenceComponent::OnDetached() {
		// -----------------------------------------------------------------------
		// コンポーネント終了処理
		// -----------------------------------------------------------------------
		// 
		// 【実行タイミング】
		//   - コンポーネントがエンティティからデタッチされるときに、1回だけ呼び出される
		//   - エンティティが削除されるときにも呼び出される
		// 
		// 【典型的な処理】
		//   1. リソースの解放
		//   2. メモリの削除
		//   3. ファイルのクローズ
		//   4. ポインタのnullptr初期化
		//   5. イベントリスナーの登録解除
		//   6. ServiceLocator 経由のサービスの登録解除
		// 
		// 【例】
		//   if (_allocatedMemory) {
		//       delete _allocatedMemory;
		//       _allocatedMemory = nullptr;
		//   }
		//   _anotherComponent = nullptr;
		//

		// NOTE: クリーンアップが必要な場合、ここに記述
		_sampleValue = 0.0f;
		_sampleString = "default";
	}

	// =========================================================================
	// 公開インターフェース メソッドの実装
	// =========================================================================

	void ReferenceComponent::SetSampleValue(float value) {
		// -----------------------------------------------------------------------
		// 【メソッド説明】
		//   コンポーネント固有のプロパティを設定するメソッドの例
		// 
		// 【パラメータ】
		//   value: 設定する値
		// 
		// 【使用例】
		//   auto* component = entity->GetComponent<ReferenceComponent>();
		//   component->SetSampleValue(5.0f);
		// 

		_sampleValue = value;
	}

	float ReferenceComponent::GetSampleValue() const {
		// -----------------------------------------------------------------------
		// 【メソッド説明】
		//   コンポーネント固有のプロパティを取得するメソッドの例
		// 
		// 【戻り値】
		//   現在の _sampleValue
		// 
		// 【使用例】
		//   float value = component->GetSampleValue();
		// 
		// 【重要】
		//   戻り値を使用しないと警告が出るように [[nodiscard]] 属性をつけています
		// 

		return _sampleValue;
	}

	// =========================================================================
	// BaseComponent の必須オーバーライド
	// =========================================================================

	std::string_view ReferenceComponent::GetStableName() const {
		// -----------------------------------------------------------------------
		// 【メソッド説明】
		//   コンポーネントの一意の識別子を返す
		//   この文字列は JSON ファイルに保存されるため、変更厳禁！
		// 
		// 【形式】
		//   "namespace.ComponentName"
		//   - namespace: プロジェクト名や機能グループ（小文字推奨）
		//   - ComponentName: コンポーネント名（PascalCase）
		// 
		// 【命名例】
		//   "mygame.PlayerControlComponent"
		//   "mygame.gameplay.EnemyAIComponent"
		//   "mygame.ui.ButtonComponent"
		// 
		// 【重要な注意】
		//   1. 全プロジェクト内で一意であること（重複厳禁）
		//   2. 一度設定したら変更しない（JSON 互換性が損なわれる）
		//   3. 新しいコンポーネントを作成するときは、名前の競合をチェック
		// 

		return "mygame.ReferenceComponent";
	}

	std::string_view ReferenceComponent::GetComponentName() const {
		// -----------------------------------------------------------------------
		// 【メソッド説明】
		//   コンポーネントの表示名を返す
		//   エディタのインスペクター画面に表示されます
		// 
		// 【命名例】
		//   "Player Control Component"
		//   "Enemy AI Component"
		//   "Button Component"
		// 
		// 【特徴】
		//   - GetStableName() と異なり、変更しても JSON に影響しない
		//   - ユーザーが読みやすい表示を心がける
		//   - スペースを含めて OK
		// 

		return "Reference Component";
	}

#ifdef _DEBUG
	void ReferenceComponent::DrawInspectorImGui() {
		// -----------------------------------------------------------------------
		// 【メソッド説明】
		//   ImGui を使用してインスペクター画面を描画
		//   このメソッドは _DEBUG ビルドでのみ呼び出されます
		// 
		// 【実行タイミング】
		//   - エディタの UI フレームで毎フレーム呼び出される
		// 
		// 【典型的な使用法】
		//   1. コンポーネントのプロパティを表示
		//   2. スライダーで値を調整
		//   3. ボタンで機能をテスト
		//   4. デバッグ用の情報を表示
		// 
		// 【ImGui の主な関数】
		//   - ImGui::Text(...): テキスト表示
		//   - ImGui::SliderFloat(...): スライダー
		//   - ImGui::InputFloat(...): 数値入力
		//   - ImGui::InputText(...): テキスト入力
		//   - ImGui::Button(...): ボタン
		//   - ImGui::Checkbox(...): チェックボックス
		//   - ImGui::Separator(): 区切り線
		//   - ImGui::TextColored(...): 色付きテキスト
		//   - ImGui::Combo(...): ドロップダウン
		//
		// 【カラーの例】
		//   赤:   ImVec4(1.0f, 0.0f, 0.0f, 1.0f)
		//   緑:   ImVec4(0.0f, 1.0f, 0.0f, 1.0f)
		//   青:   ImVec4(0.0f, 0.0f, 1.0f, 1.0f)
		//   黄:   ImVec4(1.0f, 1.0f, 0.0f, 1.0f)
		// 

		// コンポーネント名のヘッダー
		ImGui::Text("=== Reference Component ===");

		ImGui::Separator();
		ImGui::Text("Component Properties");

		// サンプル値のスライダー
		ImGui::SliderFloat("Sample Value", &_sampleValue, 0.0f, 10.0f, "%.2f");

		// サンプル文字列の入力
		ImGui::InputText("Sample String", &_sampleString);

		ImGui::Separator();
		ImGui::Text("Debug Information");

		// 現在の値を表示
		ImGui::Text("Current Value: %.4f", _sampleValue);
		ImGui::Text("String Length: %zu", _sampleString.length());

		ImGui::Separator();

		// テストボタン
		if (ImGui::Button("Test Button", ImVec2(200, 0))) {
			// NOTE: ボタンが押されたときの処理
			_sampleValue = 0.0f;
		}

		ImGui::SameLine();
		if (ImGui::Button("Reset", ImVec2(100, 0))) {
			// NOTE: リセットボタン
			_sampleValue = 0.0f;
			_sampleString = "default";
		}
	}
#endif

	// =========================================================================
	// JSON シリアライゼーション
	// =========================================================================

	void ReferenceComponent::Deserialize(const Unnamed::JsonReader& reader) {
		// -----------------------------------------------------------------------
		// 【メソッド説明】
		//   JSON ファイルからコンポーネントのデータを読み込む
		//   シーンファイルが読み込まれるときに、エンジンから呼び出されます
		// 
		// 【使用パターン】
		//   
		//   // 基本的な使い方
		//   if (auto val = reader.Read<float>("propertyName")) {
		//       _memberVariable = val.value();
		//   }
		//
		//   // 複数の型に対応
		//   reader.Read<int>(...)         // 整数
		//   reader.Read<float>(...)       // 小数
		//   reader.Read<bool>(...)        // 真偽値
		//   reader.Read<std::string>(...) // 文字列
		//   reader.Read<Vec3>(...)        // 3Dベクトル
		// 
		// 【重要な注意】
		//   1. optional<T> で値が返るため、has_value() または .value() で確認
		//   2. JSON に無いキーを読もうとしても、nullopt が返るだけ
		//   3. 必ずデフォルト値を設定して、JSON に無い場合に対応
		//   4. Serialize() で書き込む key と同じ名前を使用
		// 
		// 【JSON の例】
		//   {
		//       "sampleValue": 5.5,
		//       "sampleString": "hello"
		//   }
		// 

		// NOTE: JSON から値を読み込む例

		// サンプル値の読み込み
		if (auto val = reader.Read<float>("sampleValue")) {
			_sampleValue = val.value();
		}
		// JSON に無い場合は、_sampleValue のデフォルト値が使用される

		// サンプル文字列の読み込み
		if (auto val = reader.Read<std::string>("sampleString")) {
			_sampleString = val.value();
		}
		// JSON に無い場合は、_sampleString のデフォルト値 "default" が使用される
	}

	void ReferenceComponent::Serialize(Unnamed::JsonWriter& writer) const {
		// -----------------------------------------------------------------------
		// 【メソッド説明】
		//   コンポーネントのデータを JSON ファイルに書き込む
		//   エディタで「保存」するときに、エンジンから呼び出されます
		// 
		// 【使用パターン】
		//
		//   writer.Key("propertyName");       // JSON のキーを指定
		//   writer.Write(_memberVariable);    // 値を書き込む
		// 
		// 【複数のプロパティを書き込む】
		//
		//   writer.Key("property1");
		//   writer.Write(_value1);
		//
		//   writer.Key("property2");
		//   writer.Write(_value2);
		// 
		// 【重要な注意】
		//   1. Deserialize() で読み込む key と名前を一致させてください
		//   2. すべての重要なメンバ変数を保存してください
		//   3. 計算結果など、他から導出できる値は保存不要
		// 
		// 【出力される JSON の例】
		//   {
		//       "sampleValue": 5.5,
		//       "sampleString": "hello"
		//   }
		// 

		// NOTE: 値を JSON に書き込む例

		// サンプル値を書き込む
		writer.Key("sampleValue");
		writer.Write(_sampleValue);

		// サンプル文字列を書き込む
		writer.Key("sampleString");
		writer.Write(_sampleString);
	}

	// =========================================================================
	// コンポーネント登録（必須）
	// =========================================================================

	// NOTE: 【重要】このマクロを忘れるとコンポーネントが登録されません！
	//       コンポーネント実装ファイルの最後に必ず記述してください。
	//
	// 【何をしているのか】
	//   - このマクロは、コンポーネントをエンジンに登録します
	//   - RegisterComponent 関数を呼び出し、GetStableName() を登録
	//   - エンジンがコンポーネントを作成・削除できるようにします
	//
	// 【用途】
	//   - JSON ファイルからコンポーネントを復元するとき
	//   - エディタでコンポーネントを追加するとき
	//   - プログラムからコンポーネントを動的に生成するとき
	//
	// 【重要な注意】
	//   1. このマクロを忘れるとコンポーネントが動作しません（デバッグが難しい）
	//   2. 新しいコンポーネントを作成したら必ず追加してください
	//   3. ファイルの最後に記述することを推奨
	//

	REGISTER_COMPONENT(ReferenceComponent);

} // namespace MyGame

/*
================================================================================
                    実装ファイル内のセクション構成
================================================================================

このファイルは、以下の順序で構成されています：

1. インクルード
   - 必要なヘッダーファイルをインクルード

2. ライフサイクル メソッド
   - OnAttached():  初期化処理
   - OnTick():      毎フレーム更新
   - OnDetached():  終了処理

3. 公開インターフェース メソッド
   - SetXxx(), GetXxx() などのメソッド

4. BaseComponent の必須オーバーライド
   - GetStableName():      一意の識別子
   - GetComponentName():   表示名
   - DrawInspectorImGui(): デバッグ UI
   - Deserialize():        JSON 読み込み
   - Serialize():          JSON 書き込み

5. コンポーネント登録
   - REGISTER_COMPONENT() マクロ

【推奨事項】
  新しいコンポーネントを作成するときも、このセクション構成に従うと
  コードの可読性と保守性が向上します。

================================================================================
*/
