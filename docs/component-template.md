# Component Documentation Template

このページは、エンジン内コンポーネントの説明を統一フォーマットで記述するためのテンプレートです。  
`[]` で囲まれたプレースホルダーを置き換えて使用してください。

## 使い方
1. ファイル名を `lowercasecomponentname.md` などに変更してコピーする。
2. 各プレースホルダー（例: `[ComponentName]`）を実装に合わせて置換する。
3. 不要なセクションは削除し、必要なセクションを追加する。

---

# [ComponentName]

`[ComponentName]` は [Entity](entity.md) / UI Widget にアタッチされる [コンポーネント](component.md) です。  
主に `[1行で要約]` を担当します。

## 役割
- [責務1]
- [責務2]
- [責務3]

## 対象と前提
- 種別: `BaseComponent` / `UiComponent`
- `stableName` / `typeName`: `[engine.Xxx / game.Xxx / UiXxx]`
- 主な利用シーン: `[ゲーム内 / エディタ内 / 両方]`
- 依存コンポーネント・サービス: `[TransformComponent, AssetManager, ...]`

## ライフサイクルと更新タイミング

| コールバック | タイミング | 役割 |
|---|---|---|
| `OnAttached()` | [例: Entity へ追加直後] | [初期化内容] |
| `OnTick(float deltaTime)` | [例: GAMEPLAY ティック] | [主処理] |
| `OnRenderTick(...)` | [必要な場合のみ] | [描画向け更新] |
| `OnDetached()` | [例: 削除直前] | [解放・解除処理] |

## API クイックリファレンス

### 取得系
| 戻り値 | メソッド | 説明 |
|---|---|---|
| `[Type]` | `[Getter()]` | [説明] |

### 設定・操作系
| 戻り値 | メソッド | 説明 |
|---|---|---|
| `void` | `[SetterOrCommand(...)]` | [説明] |

## シリアライズ項目

| キー | 型 | 既定値 | 説明 |
|---|---|---|---|
| `[jsonKey]` | `[number/string/bool/... ]` | `[default]` | [意味] |

## エディタ・デバッグ
- Inspector 表示: `[あり / なし]`
- ImGui 利用有無: `[あり / なし]`
- `DrawInspectorImGui` を実装する場合は `#ifdef _DEBUG` で囲む（Develop/Release で ImGui 実装がリンクされないため）。

## 実装メモ
- 登録方法: `REGISTER_COMPONENT([ComponentName])` / `[別の登録経路]`
- `stableName` / `typeName` はアセット互換性キー。既存データがある状態での変更は避ける。
- [他に重要な運用上の注意]

## 参照実装・関連ファイル
- `src/[path/to/header].h`
- `src/[path/to/source].cpp`
- `src/[path/to/registration].cpp`
