# <svg xmlns="http://www.w3.org/2000/svg" height="40px" viewBox="0 -960 960 960" width="40px" fill="#e3e3e3"><path d="M480-80 314.67-245.33 363-293.67 446.67-210v-236.67H210.33l80.34 80L242-318 80-480l162.33-162.33L290.67-594 210-513.33h236.67V-750l-80.34 80.33L318-718l162-162 162 162-48.33 48.33L513.33-750v236.67h236.34l-80.34-80L718-642l162 162-162 162-48.33-48.33L750-446.67H513.33v236.34L596.67-294l48.66 48.67L480-80Z"/></svg> TransformComponent


`TransformComponent` は [Entity](entity.md) の姿勢（位置・回転・スケール）を管理する [コンポーネント](component.md) です。  
基本的にすべての Entity が 1 つ持つ前提で設計されています。

## 1. 役割
- ローカル姿勢の保持（`position / rotation / scale`）
- 親子階層の管理（`SetParent`、遅延親解決）
- シミュレーション姿勢と描画姿勢の橋渡し（固定ティック補間）

## 2. 座標系の整理
- ローカル値: 親基準の姿勢
- ワールド値: ルート基準の姿勢
- `GetWorldMat()`: シミュレーションで確定した姿勢
- `RenderWorldMat()`: 描画補間を反映した姿勢（描画フェーズで使用）

## 3. API クイックリファレンス

### 姿勢の取得/設定
| 戻り値 | メソッド | 説明 |
|---|---|---|
| `Vec3` | `GetPosition()` | ローカル位置を取得します。 |
| `Quaternion` | `GetRotation()` | ローカル回転を取得します。 |
| `Vec3` | `GetScale()` | ローカルスケールを取得します。 |
| `void` | `SetPosition(Vec3 position)` | ローカル位置を設定します。 |
| `void` | `SetRotation(Quaternion rotation)` | ローカル回転を設定します。 |
| `void` | `SetScale(Vec3 scale)` | ローカルスケールを設定します。 |

### 親子階層
| 戻り値 | メソッド | 説明 |
|---|---|---|
| `TransformComponent*` | `GetParent()` | 親 Transform を取得します。 |
| `void` | `SetParent(TransformComponent* parent, bool preserveWorld = true)` | 親を設定します。`preserveWorld=true` ならワールド姿勢を維持します。 |
| `void` | `ResolveDeferredParent(const Scene& scene)` | シーンロード後に親参照を解決します。 |

### 行列・方向ベクトル
| 戻り値 | メソッド | 説明 |
|---|---|---|
| `const Mat4&` | `GetWorldMat()` | シミュレーション用ワールド行列を取得します。 |
| `const Mat4&` | `RenderWorldMat()` | 描画用ワールド行列を取得します（補間反映）。 |
| `Vec3` | `Right()` | ワールド右方向ベクトルを取得します。 |
| `Vec3` | `Up()` | ワールド上方向ベクトルを取得します。 |
| `Vec3` | `Forward()` | ワールド前方向ベクトルを取得します。 |

### 補間制御
| 戻り値 | メソッド | 説明 |
|---|---|---|
| `void` | `RequestInterpolationResync()` | 補間履歴を再同期します。瞬間移動や大きな補正後に使用します。 |
