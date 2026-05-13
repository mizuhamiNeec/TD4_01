# MagVoiceBridge 使用ガイド

## 目次
1. [概要](#1-概要)
2. [セットアップ・初期化](#2-セットアップ初期化)
3. [基本的な使用方法](#3-基本的な使用方法)
4. [API リファレンス](#4-api-リファレンス)
5. [使用例](#5-使用例)
6. [よくある質問](#6-よくある質問)
7. [トラブルシューティング](#7-トラブルシューティング)

---

## 1. 概要

### MagVoiceBridge とは

`MagVoiceBridge`は、Windows の **WASAPI（Windows Audio Session API）** を使用して、デフォルトマイクからリアルタイムに音声入力を取得し、以下の機能を提供するC++クラスです。

#### 主な機能

| 機能 | 説明 |
|------|------|
| **音声キャプチャ** | マイクからリアルタイムに音声データを取得 |
| **音量計測** | RMS値、dB値、ピークレベル、スムージング済み値等を計測 |
| **周波数分析** | ゼロクロス率（ZCR）から周波数特性を分析 |
| **音声検出** | ノイズと人の声を区別するAI的な判定 |
| **パラメータ調整** | マイク感度やノイズフロアを環境に応じて調整可能 |
| **スレッドセーフ** | std::atomic と std::mutex を使用した安全な操作 |

#### 使用シーン

- **ゲーム**: マイク入力によるボイスチャット、音量表示メーター
- **配信**: リアルタイム音量表示、ノイズゲート
- **音声認識**: 前処理としての音声/ノイズ判別
- **オーディオアナライザー**: リアルタイム音量分析
- **VR/AR**: インタラクティブなマイク入力処理

### システム要件

| 項目 | 要件 |
|------|------|
| **OS** | Windows 7 以上 |
| **C++ バージョン** | C++11 以上 |
| **必須ライブラリ** | Windows SDK（WASAPI） |
| **コンパイラ** | Visual Studio 2015 以上 |
| **ハードウェア** | マイク入力対応デバイス |

### クラス概要

```cpp
class MagVoiceBridge {
public:
    // 初期化・制御
    MagVoiceBridge();
    ~MagVoiceBridge();
    bool Initialize();           // WASAPI初期化
    bool Start();                // キャプチャ開始
    void Stop();                 // キャプチャ停止
    void Shutdown();             // リソース解放
    void Update();               // 毎フレーム呼び出し

    // 音声データ取得
    std::vector<float> GetSamples();     // 音声サンプル
    void ClearSamples();                 // バッファクリア
    uint32_t GetSampleRate() const;      // サンプリングレート
    uint16_t GetChannelCount() const;    // チャンネル数

    // 音量取得
    float GetCurrentVolume();            // 現在の音量（0.0-1.0）
    float GetCurrentVolumeDB();          // 現在の音量（dB）
    float GetPeakVolume();               // ピークレベル（0.0-1.0）
    float GetPeakVolumeDB();             // ピークレベル（dB）
    float GetSmoothedVolume();           // スムージング済み音量（0.0-1.0）
    float GetSmoothedVolumeDB();         // スムージング済み音量（dB）
    float GetVolumePercentage();         // 音量（0-100%）

    // 音声検出
    bool IsVoiceDetected();              // 人の声判定
    float CalculateZeroCrossingRate();   // ゼロクロス率
    float GetVoiceCharacteristicScore(); // 音声特性スコア

    // パラメータ調整
    void SetSmoothingFactor(float factor);
    void SetVolumeRange(float minDb, float maxDb);
    void SetNoiseFloor(float noiseFloorDB);
    void SetVoiceDetectionThresholds(float zcRate, float volumeDB);

    // 統計情報
    VolumeStats GetVolumeStats();
};
```

---

## 2. セットアップ・初期化

### プロジェクトへの組み込み

#### ステップ 1: ファイルの追加

プロジェクトに以下の 2 ファイルを追加します：

```
project/
├── MagVoiceBridge.h
├── MagVoiceBridge.cpp
└── ...
```

#### ステップ 2: Visual Studio プロジェクト設定

**リンカー設定**で以下のライブラリをリンクします：

```
Ole32.lib      (COM ライブラリ)
Uuid.lib       (UUID ライブラリ)
```

Visual Studio での設定：
1. プロジェクトプロパティ → リンカー → 入力
2. 追加の依存関係に上記を追加

#### ステップ 3: ヘッダーのインクルード

```cpp
#include "MagVoiceBridge.h"
```

### 初期化フロー

```
┌─────────────────────────────────────────┐
│   MagVoiceBridge インスタンス作成        │
│   MagVoiceBridge bridge;                │
└─────────────┬───────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│   Initialize() 呼び出し                  │
│   WASAPI 初期化、マイク取得              │
└─────────────┬───────────────────────────┘
              │
              ▼ (成功時)
┌─────────────────────────────────────────┐
│   Start() 呼び出し                       │
│   オーディオストリーム開始                │
└─────────────┬───────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│   Update() を毎フレーム呼び出し           │
│   音声データ取得、分析                    │
└─────────────┬───────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│   Stop() 呼び出し                        │
│   キャプチャ停止                         │
└─────────────┬───────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│   Shutdown() 呼び出し                    │
│   リソース解放                           │
└─────────────┴───────────────────────────┘
```

### 基本的な初期化コード

```cpp
// インスタンス生成
MagVoiceBridge voiceBridge;

// 初期化
if (!voiceBridge.Initialize()) {
    // エラー処理
    return false;
}

// キャプチャ開始
if (!voiceBridge.Start()) {
    // エラー処理
    return false;
}

// 以降、Update() を毎フレーム呼び出し
// voiceBridge.Update();

// 終了時
voiceBridge.Stop();
voiceBridge.Shutdown();
```

### グローバル/メンバ変数での保持

**方法1: グローバル変数（シンプル）**
```cpp
MagVoiceBridge g_voiceBridge;  // グローバル変数

void Initialize() {
    g_voiceBridge.Initialize();
    g_voiceBridge.Start();
}

void Shutdown() {
    g_voiceBridge.Stop();
    g_voiceBridge.Shutdown();
}
```

**方法2: std::unique_ptr（推奨）**
```cpp
std::unique_ptr<MagVoiceBridge> g_voiceBridge;

void Initialize() {
    g_voiceBridge = std::make_unique<MagVoiceBridge>();
    if (!g_voiceBridge->Initialize()) {
        g_voiceBridge.reset();
        return;
    }
    g_voiceBridge->Start();
}

void Shutdown() {
    if (g_voiceBridge) {
        g_voiceBridge->Stop();
        g_voiceBridge->Shutdown();
        g_voiceBridge.reset();
    }
}
```

**方法3: クラスメンバ（エンジン統合）**
```cpp
class AudioManager {
private:
    MagVoiceBridge voiceBridge_;

public:
    bool Initialize() {
        return voiceBridge_.Initialize() && voiceBridge_.Start();
    }

    void Update() {
        voiceBridge_.Update();
    }

    void Shutdown() {
        voiceBridge_.Stop();
        voiceBridge_.Shutdown();
    }
};
```

---

## 3. 基本的な使用方法

### 最小限の使用例

```cpp
#include "MagVoiceBridge.h"
#include <iostream>

int main() {
    MagVoiceBridge voiceBridge;

    // 初期化
    if (!voiceBridge.Initialize()) {
        std::cerr << "Initialize failed" << std::endl;
        return 1;
    }

    // キャプチャ開始
    if (!voiceBridge.Start()) {
        std::cerr << "Start failed" << std::endl;
        voiceBridge.Shutdown();
        return 1;
    }

    // 100フレーム音声取得
    for (int i = 0; i < 100; ++i) {
        voiceBridge.Update();

        float volume = voiceBridge.GetCurrentVolume();
        float volumeDB = voiceBridge.GetCurrentVolumeDB();
        bool isVoice = voiceBridge.IsVoiceDetected();

        printf("Volume: %.2f (%.1f dB), Voice: %s\n",
               volume, volumeDB, isVoice ? "YES" : "NO");
    }

    // クリーンアップ
    voiceBridge.Stop();
    voiceBridge.Shutdown();

    return 0;
}
```

### 毎フレーム処理パターン

```cpp
class GameEngine {
private:
    MagVoiceBridge voiceBridge_;

public:
    void Initialize() {
        voiceBridge_.Initialize();
        voiceBridge_.Start();
    }

    void Update() {
        // 音声更新
        voiceBridge_.Update();

        // 音量取得
        float currentVolume = voiceBridge_.GetCurrentVolume();
        float smoothedVolume = voiceBridge_.GetSmoothedVolume();

        // UI更新（例：音量メーター）
        UpdateVolumeMeter(smoothedVolume);

        // 音声検出判定
        if (voiceBridge_.IsVoiceDetected()) {
            OnVoiceDetected();
        }
    }

    void Shutdown() {
        voiceBridge_.Stop();
        voiceBridge_.Shutdown();
    }
};
```

### Update() 呼び出しの重要性

**NOTE: Update() は必ず毎フレーム（またはスレッド内）で定期的に呼び出してください**

```cpp
// フレームループ内
while (isRunning) {
    // 毎フレーム必ず呼び出す
    voiceBridge.Update();

    // その他の処理
    Render();
}
```

---

## 4. API リファレンス

### 4.1 初期化・制御メソッド

#### `bool Initialize()`

**説明**: WASAPI を初期化し、デフォルト録音デバイスを準備します。

**戻り値**:
- `true`: 初期化成功
- `false`: 初期化失敗

**呼び出し条件**:
- インスタンス作成直後に呼び出し
- `Start()` の前に呼び出し

**使用例**:
```cpp
MagVoiceBridge voiceBridge;
if (!voiceBridge.Initialize()) {
    // エラー処理：デバイス接続確認等
}
```

**備考**:
- マイクが接続されていない場合は失敗
- 既に初期化済みの状態で呼び出すと失敗

---

#### `bool Start()`

**説明**: オーディオストリームを開始し、キャプチャを開始します。

**戻り値**:
- `true`: 開始成功
- `false`: 開始失敗

**呼び出し条件**:
- `Initialize()` 成功後に呼び出し
- `Stop()` の後に再度呼び出し可能

**使用例**:
```cpp
if (!voiceBridge.Start()) {
    // エラー処理
}
```

**備考**:
- `Update()` を呼び出す前に `Start()` を完了してください
- `Start()` なしで `Update()` を呼び出しても何も処理されません

---

#### `void Stop()`

**説明**: オーディオストリームを停止します。

**戻り値**: なし

**呼び出し条件**:
- `Start()` 後に呼び出し可能
- `Shutdown()` の前に呼び出すことを推奨

**使用例**:
```cpp
voiceBridge.Stop();
```

**備考**:
- `Stop()` は冪等性を持つ（複数回呼び出しても安全）
- `Stop()` 後は `Update()` を呼び出しても処理されません

---

#### `void Shutdown()`

**説明**: すべてのリソースを解放し、システムをクリーンアップします。

**戻り値**: なし

**呼び出し条件**:
- `Stop()` の後に呼び出すことを推奨
- インスタンス破棄直前に呼び出し

**使用例**:
```cpp
voiceBridge.Stop();
voiceBridge.Shutdown();
```

**備考**:
- `Shutdown()` は冪等性を持つ（複数回呼び出しても安全）
- デストラクタで自動的に呼び出されます

---

#### `void Update()`

**説明**: WASAPI から最新のオーディオデータを取得し、分析を実行します。

**戻り値**: なし

**呼び出し条件**:
- `Start()` 成功後に毎フレーム（または定期的に）呼び出し

**呼び出し頻度**:
- 毎フレーム（60Hz 以上推奨）
- または専用スレッド内で定期呼び出し

**使用例**:
```cpp
// メインループ内
while (isRunning) {
    voiceBridge.Update();

    float volume = voiceBridge.GetCurrentVolume();
    // UI更新等
}
```

**備考**:
- `Update()` 内で計算量が多い場合は別スレッド化を検討
- `Update()` はスレッドセーフです

---

### 4.2 音声データ取得メソッド

#### `std::vector<float> GetSamples()`

**説明**: 現在保持している音声サンプルバッファをコピーして返します。

**戻り値**: 
- `std::vector<float>`: 音声サンプル（-1.0 ～ 1.0）
- 正規化済み float 値

**サンプル範囲**:
- 最大 10 秒分（48kHz で約 480,000 サンプル）
- メモリ肥大化防止のため自動削除

**使用例**:
```cpp
std::vector<float> samples = voiceBridge.GetSamples();
for (float sample : samples) {
    printf("Sample: %.4f\n", sample);
}
```

**備考**:
- データコピーなため重い処理（少度合に呼び出し推奨）
- スレッドセーフです

---

#### `void ClearSamples()`

**説明**: 保持しているサンプルバッファをクリアします。

**戻り値**: なし

**使用シーン**:
- 新しい記録を開始するとき
- メモリを明示的に解放したいとき

**使用例**:
```cpp
// 新しい記録開始
voiceBridge.ClearSamples();
voiceBridge.Update();
```

**備考**:
- `GetSamples()` で取得したデータは保持されます
- 自動削除と併用しても安全

---

#### `uint32_t GetSampleRate() const`

**説明**: マイクのサンプリングレートを取得します。

**戻り値**: 
- `uint32_t`: Hz 単位（通常 48000Hz）

**使用例**:
```cpp
uint32_t sampleRate = voiceBridge.GetSampleRate();
printf("Sample Rate: %u Hz\n", sampleRate);

// 周波数分析での使用
float freq = (sampleIndex * sampleRate) / bufferSize;
```

**備考**:
- `Initialize()` 前は 0 を返す
- 通常は 44100Hz または 48000Hz

---

#### `uint16_t GetChannelCount() const`

**説明**: マイクのチャンネル数を取得します。

**戻り値**:
- `uint16_t`: チャンネル数（1=モノラル、2=ステレオ）

**使用例**:
```cpp
uint16_t channels = voiceBridge.GetChannelCount();
if (channels == 1) {
    printf("Mono\n");
} else if (channels == 2) {
    printf("Stereo\n");
}
```

**備考**:
- `Initialize()` 前は 0 を返す
- 通常は 2（ステレオ）

---

### 4.3 音量取得メソッド

#### `float GetCurrentVolume()`

**説明**: 現在の音量を 0.0～1.0 の範囲で返します（RMS 値）。

**戻り値**:
- `float`: 0.0（無音） ～ 1.0（最大）

**使用例**:
```cpp
float volume = voiceBridge.GetCurrentVolume();
printf("Volume: %.2f\n", volume);  // 例: 0.45

// UI メーター（0-100%）
int meterLevel = static_cast<int>(volume * 100);
```

**計算方法**:
```
RMS = sqrt( sum(sample^2) / N )
```

**特性**:
- リアルタイム値（変動が大きい）
- スムージングなし

**推奨用途**:
- 高精度な分析
- リアルタイムメーター

---

#### `float GetCurrentVolumeDB()`

**説明**: 現在の音量を dB 単位で返します。

**戻り値**:
- `float`: dB 値（通常 -80 ～ 0dB）

**使用例**:
```cpp
float volumeDB = voiceBridge.GetCurrentVolumeDB();
printf("Volume: %.1f dB\n", volumeDB);  // 例: -20.5 dB
```

**計算方法**:
```
dB = 20 * log10(RMS)
```

**参考値**:
| 状態 | dB 値 |
|------|------|
| 無音 | -80 dB |
| かすかな音 | -60 dB |
| 静かな声 | -40 dB |
| 通常の会話 | -20 dB |
| 大声 | -6 dB |

**推奨用途**:
- デシベル単位での表示
- プロフェッショナルなツール

---

#### `float GetPeakVolume()`

**説明**: 現在のピークレベル（最大振幅）を 0.0～1.0 で返します。

**戻り値**:
- `float`: 0.0～1.0

**使用例**:
```cpp
float peak = voiceBridge.GetPeakVolume();
printf("Peak: %.2f\n", peak);

// ピークメーター表示
if (peak > 0.9) {
    printf("WARNING: Clipping risk!\n");
}
```

**特性**:
- 最新 2048 サンプル内の最大値
- リアルタイム値

**推奨用途**:
- ピークメーター表示
- クリッピング警告

---

#### `float GetPeakVolumeDB()`

**説明**: 現在のピークレベルを dB 単位で返します。

**戻り値**:
- `float`: dB 値

**使用例**:
```cpp
float peakDB = voiceBridge.GetPeakVolumeDB();
printf("Peak: %.1f dB\n", peakDB);
```

**計算方法**:
```
dB = 20 * log10(PeakLevel)
```

---

#### `float GetSmoothedVolume()`

**説明**: スムージング（移動平均）された音量を 0.0～1.0 で返します。

**戻り値**:
- `float`: 0.0～1.0

**使用例**:
```cpp
float smoothedVolume = voiceBridge.GetSmoothedVolume();

// UI メーター（安定した表示）
DisplayVolumeMeter(smoothedVolume);
```

**特性**:
- 変動を平滑化
- UI ちらつき低減
- デフォルト係数: 0.4（調整可能）

**計算方法**:
```
smoothed = smoothed * (1 - factor) + current * factor
```

**推奨用途**:
- UI 表示（メーター）
- ゲーム内表示

---

#### `float GetSmoothedVolumeDB()`

**説明**: スムージング済み音量を dB 単位で返します。

**戻り値**:
- `float`: dB 値

**使用例**:
```cpp
float smoothedDB = voiceBridge.GetSmoothedVolumeDB();
printf("Smoothed: %.1f dB\n", smoothedDB);
```

---

#### `float GetVolumePercentage()`

**説明**: 音量をパーセンテージ（0～100）で返します。

**戻り値**:
- `float`: 0.0～100.0 %

**使用例**:
```cpp
float percentage = voiceBridge.GetVolumePercentage();
printf("Volume: %.1f%%\n", percentage);

// プログレスバー表示
DrawProgressBar(percentage);
```

**計算方法**:
```
normalized = (dB - minDB) / (maxDB - minDB)
percentage = normalized * 100
```

**デフォルト範囲**:
- Min: -60 dB
- Max: -6 dB

**調整方法**:
```cpp
voiceBridge.SetVolumeRange(-70.0f, -3.0f);
```

---

### 4.4 音声検出メソッド

#### `bool IsVoiceDetected()`

**説明**: 人の声かどうかを判定します。

**戻り値**:
- `true`: 人の声の可能性あり
- `false`: 音声未検出（無音またはノイズ）

**使用例**:
```cpp
if (voiceBridge.IsVoiceDetected()) {
    printf("Voice detected!\n");
    OnVoiceInput();
} else {
    printf("No voice or noise\n");
}
```

**判定ロジック**:
```
条件1: ゼロクロス率 < 閾値
条件2: 音量 > ノイズフロア

isVoice = (ZCR < threshold) && (volumeDB > noiseFloor)
```

**判定基準**:
| 信号 | ZCR | 音量 | 判定 |
|------|-----|------|------|
| 無音 | 低 | 低 | ✗ |
| 人の声 | 低 | 高 | ✓ |
| 背景ノイズ | 高 | 中 | ✗ |
| 大声 | 低 | 高 | ✓ |

**推奨用途**:
- ボイスチャット判定
- ノイズゲート
- 音声認識前処理

---

#### `float CalculateZeroCrossingRate()`

**説明**: ゼロクロス率（ZCR）を計算します（0.0～1.0）。

**戻り値**:
- `float`: ゼロクロス率（0.0～1.0）

**使用例**:
```cpp
float zcr = voiceBridge.CalculateZeroCrossingRate();
printf("ZCR: %.3f\n", zcr);

// 周波数情報を参考
if (zcr < 0.15) {
    printf("Low frequency (voice-like)\n");
} else if (zcr > 0.40) {
    printf("High frequency (noise)\n");
}
```

**計算方法**:
```
ZCR = (符号変化の回数) / (サンプル数 - 1)
```

**参考値**:
| 信号 | ZCR 値 |
|------|--------|
| 人の音声 | 0.05～0.20 |
| 背景ノイズ | 0.30～0.50 |
| ホワイトノイズ | 0.40～0.60 |

**特性**:
- 最新 2048 サンプルで計算
- 周波数特性を反映
- 調整可能（`SetVoiceDetectionThresholds`）

**推奨用途**:
- 周波数分析
- ノイズ/音声判別
- デバッグ情報

---

#### `float GetVoiceCharacteristicScore()`

**説明**: 音声特性スコア（0.0～1.0）を返します。

**戻り値**:
- `float`: 0.0（ノイズっぽい） ～ 1.0（人の声っぽい）

**使用例**:
```cpp
float voiceScore = voiceBridge.GetVoiceCharacteristicScore();
printf("Voice Score: %.2f\n", voiceScore);

// 信頼度表示
if (voiceScore > 0.8) {
    printf("Confidence: High\n");
} else if (voiceScore > 0.5) {
    printf("Confidence: Medium\n");
} else {
    printf("Confidence: Low\n");
}
```

**計算方法**:
```
voiceScore = max(0.0, 1.0 - (ZCR / THRESHOLD))
```

**特性**:
- 1.0 に近い = 人の声らしい
- 0.0 に近い = ノイズっぽい

**推奨用途**:
- 検出信頼度の可視化
- UI フィードバック

---

### 4.5 パラメータ調整メソッド

#### `void SetSmoothingFactor(float factor)`

**説明**: スムージング係数を設定します（0.0～1.0）。

**パラメータ**:
- `factor`: スムージング係数
  - 0.0: 最も反応的（変動が大きい）
  - 0.5: バランス
  - 1.0: 最も安定（反応が遅い）

**使用例**:
```cpp
// リアルタイム性重視
voiceBridge.SetSmoothingFactor(0.1f);

// UI 安定性重視
voiceBridge.SetSmoothingFactor(0.7f);

// バランス（デフォルト）
voiceBridge.SetSmoothingFactor(0.4f);
```

**デフォルト値**: 0.4

**計算方法**:
```
smoothed = smoothed * (1 - factor) + current * factor
```

**推奨値**:
| 用途 | 推奨値 |
|------|--------|
| 高レスポンス | 0.1～0.2 |
| バランス | 0.3～0.5 |
| 安定性重視 | 0.6～0.9 |

---

#### `void SetVolumeRange(float minDb, float maxDb)`

**説明**: 音量正規化の範囲を設定します。

**パラメータ**:
- `minDb`: 最小基準（例：-80dB = 無音）
- `maxDb`: 最大基準（例：-6dB = 大声）

**使用例**:
```cpp
// 通常のマイク感度
voiceBridge.SetVolumeRange(-60.0f, -6.0f);

// 感度が低いマイク
voiceBridge.SetVolumeRange(-80.0f, -12.0f);

// 感度が高いマイク
voiceBridge.SetVolumeRange(-50.0f, -3.0f);
```

**デフォルト値**:
- Min: -60 dB
- Max: -6 dB

**影響を受けるメソッド**:
- `GetVolumePercentage()`

**使用シーン**:
- マイク感度の調整
- 環境に応じたキャリブレーション

---

#### `void SetNoiseFloor(float noiseFloorDB)`

**説明**: ノイズフロア（最小検出音量）を設定します。

**パラメータ**:
- `noiseFloorDB`: ノイズフロアの dB 値

**使用例**:
```cpp
// 標準（-50dB 以下は無視）
voiceBridge.SetNoiseFloor(-50.0f);

// ノイズが多い環境
voiceBridge.SetNoiseFloor(-40.0f);

// 静かな環境
voiceBridge.SetNoiseFloor(-60.0f);
```

**デフォルト値**: -50 dB

**影響を受けるメソッド**:
- `IsVoiceDetected()`

**使用シーン**:
- 背景ノイズ除外
- 環境ノイズの調整

---

#### `void SetVoiceDetectionThresholds(float zcRateThreshold, float volumeThresholdDB)`

**説明**: 音声検出の閾値を設定します。

**パラメータ**:
- `zcRateThreshold`: ゼロクロス率の閾値（0.0～1.0）
- `volumeThresholdDB`: 音量の閾値（dB）

**使用例**:
```cpp
// デフォルト（バランス）
voiceBridge.SetVoiceDetectionThresholds(0.25f, -40.0f);

// より厳密な判定
voiceBridge.SetVoiceDetectionThresholds(0.20f, -35.0f);

// より緩い判定
voiceBridge.SetVoiceDetectionThresholds(0.35f, -50.0f);
```

**デフォルト値**:
- ZCR Threshold: 0.25
- Volume Threshold: -40 dB

**推奨値**:
| 環境 | ZCR | Volume dB |
|------|-----|-----------|
| 静かな環境 | 0.20 | -50 |
| 通常の会話 | 0.25 | -40 |
| ノイズ多い | 0.30 | -30 |

**影響を受けるメソッド**:
- `IsVoiceDetected()`

---

### 4.6 統計情報メソッド

#### `VolumeStats GetVolumeStats()`

**説明**: すべての音量統計情報を一括取得します。

**戻り値**: `VolumeStats` 構造体

```cpp
struct VolumeStats {
    float currentRMS{};         // 現在の RMS 値（0.0～1.0）
    float currentRMSDB{};       // 現在の RMS 値（dB）
    float peakValue{};          // ピーク値（0.0～1.0）
    float peakDB{};             // ピーク値（dB）
    float smoothedRMS{};        // スムージング済み RMS（0.0～1.0）
    float smoothedRMSDB{};      // スムージング済み RMS（dB）
    float percentage{};         // パーセンテージ（0～100）
    float voiceScore{};         // 音声特性スコア（0.0～1.0）
    bool isVoiceDetected{};     // 音声検出フラグ
};
```

**使用例**:
```cpp
MagVoiceBridge::VolumeStats stats = voiceBridge.GetVolumeStats();

printf("Current RMS: %.2f (%.1f dB)\n", stats.currentRMS, stats.currentRMSDB);
printf("Peak: %.2f (%.1f dB)\n", stats.peakValue, stats.peakDB);
printf("Smoothed: %.2f (%.1f dB)\n", stats.smoothedRMS, stats.smoothedRMSDB);
printf("Volume: %.1f%%\n", stats.percentage);
printf("Voice Score: %.2f\n", stats.voiceScore);
printf("Voice Detected: %s\n", stats.isVoiceDetected ? "YES" : "NO");
```

**利用シーン**:
- ImGui での情報表示
- ログ出力
- UI パネル更新

**メリット**:
- 複数の atomic 読み込み回数が削減される
- パフォーマンス向上

---

## 5. 使用例

### 5.1 最小限の例（5行程度）

```cpp
#include "MagVoiceBridge.h"

int main() {
    MagVoiceBridge voiceBridge;
    voiceBridge.Initialize();
    voiceBridge.Start();
    voiceBridge.Update();
    printf("Volume: %.2f\n", voiceBridge.GetSmoothedVolume());
}
```

---

### 5.2 実践的な例（音量表示メーター）

```cpp
#include "MagVoiceBridge.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    MagVoiceBridge voiceBridge;

    // 初期化
    if (!voiceBridge.Initialize()) {
        std::cerr << "Initialize failed" << std::endl;
        return 1;
    }

    // キャプチャ開始
    if (!voiceBridge.Start()) {
        std::cerr << "Start failed" << std::endl;
        voiceBridge.Shutdown();
        return 1;
    }

    std::cout << "Recording... Press Ctrl+C to stop\n";
    std::cout << "Sample Rate: " << voiceBridge.GetSampleRate() << " Hz\n";
    std::cout << "Channels: " << voiceBridge.GetChannelCount() << "\n\n";

    // メインループ（10秒間）
    for (int i = 0; i < 100; ++i) {
        voiceBridge.Update();

        // 音量統計取得
        MagVoiceBridge::VolumeStats stats = voiceBridge.GetVolumeStats();

        // コンソール出力
        printf("[%3d] ", i);
        printf("Current: %6.2f (%6.1f dB) | ", stats.currentRMS, stats.currentRMSDB);
        printf("Smoothed: %6.2f (%6.1f dB) | ", stats.smoothedRMS, stats.smoothedRMSDB);
        printf("Peak: %6.2f (%6.1f dB) | ", stats.peakValue, stats.peakDB);
        printf("Voice: %s | ", stats.isVoiceDetected ? "YES" : "NO ");
        printf("Score: %.2f\n", stats.voiceScore);

        // 100ms ウェイト
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // クリーンアップ
    voiceBridge.Stop();
    voiceBridge.Shutdown();

    std::cout << "Done\n";
    return 0;
}
```

---

### 5.3 高度なカスタマイズ例（環境依存の自動調整）

```cpp
#include "MagVoiceBridge.h"
#include <iostream>
#include <vector>
#include <algorithm>

class AdaptiveVoiceBridge {
private:
    MagVoiceBridge voiceBridge_;
    std::vector<float> peakHistory_;
    std::vector<float> zcrHistory_;
    static constexpr int HISTORY_SIZE = 50;

public:
    bool Initialize() {
        if (!voiceBridge_.Initialize()) {
            return false;
        }
        return voiceBridge_.Start();
    }

    void Update() {
        voiceBridge_.Update();

        // 統計情報取得
        MagVoiceBridge::VolumeStats stats = voiceBridge_.GetVolumeStats();

        // 履歴に追加
        peakHistory_.push_back(stats.peakDB);
        zcrHistory_.push_back(voiceBridge_.CalculateZeroCrossingRate());

        if (peakHistory_.size() > HISTORY_SIZE) {
            peakHistory_.erase(peakHistory_.begin());
            zcrHistory_.erase(zcrHistory_.begin());
        }

        // 履歴が十分に溜まったら自動調整
        if (peakHistory_.size() == HISTORY_SIZE) {
            AutoAdjustThresholds();
        }
    }

    void Shutdown() {
        voiceBridge_.Stop();
        voiceBridge_.Shutdown();
    }

private:
    void AutoAdjustThresholds() {
        // ピーク値の統計
        float avgPeak = 0.0f;
        for (float peak : peakHistory_) {
            avgPeak += peak;
        }
        avgPeak /= peakHistory_.size();

        // ZCR の統計
        float avgZcr = 0.0f;
        for (float zcr : zcrHistory_) {
            avgZcr += zcr;
        }
        avgZcr /= zcrHistory_.size();

        // ノイズフロア調整
        float noiseFloor = avgPeak - 10.0f;  // 平均より 10dB 下
        voiceBridge_.SetNoiseFloor(noiseFloor);

        // 音声検出閾値調整
        float zcThreshold = avgZcr + 0.05f;  // 平均より 0.05 上
        voiceBridge_.SetVoiceDetectionThresholds(zcThreshold, avgPeak - 5.0f);

        std::cout << "Auto-calibrated: NoiseFloor=" << noiseFloor
                  << " dB, ZCR=" << zcThreshold << std::endl;
    }

public:
    MagVoiceBridge::VolumeStats GetStats() {
        return voiceBridge_.GetVolumeStats();
    }
};

int main() {
    AdaptiveVoiceBridge bridge;

    if (!bridge.Initialize()) {
        std::cerr << "Initialization failed\n";
        return 1;
    }

    std::cout << "Running adaptive voice bridge...\n";

    for (int i = 0; i < 150; ++i) {
        bridge.Update();

        auto stats = bridge.GetStats();
        printf("[%3d] Voice: %s | Score: %.2f | Peak: %.1f dB\n",
               i, stats.isVoiceDetected ? "YES" : "NO ", stats.voiceScore, stats.peakDB);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    bridge.Shutdown();
    return 0;
}
```

---

### 5.4 ゲーム統合例（Unreal Engine スタイル）

```cpp
#include "MagVoiceBridge.h"

// ゲーム内マネージャークラス
class UAudioInputManager {
private:
    MagVoiceBridge voiceBridge_;
    bool bIsInitialized = false;

public:
    // 初期化（BeginPlay 相当）
    void Initialize() {
        if (voiceBridge_.Initialize() && voiceBridge_.Start()) {
            bIsInitialized = true;
            UE_LOG(LogAudio, Warning, TEXT("Voice bridge initialized"));
        } else {
            UE_LOG(LogAudio, Error, TEXT("Voice bridge initialization failed"));
        }
    }

    // 毎フレーム更新
    void Tick(float DeltaTime) {
        if (!bIsInitialized) return;

        voiceBridge_.Update();

        // ゲーム内で使用
        float smoothedVolume = voiceBridge_.GetSmoothedVolume();
        bool bVoiceDetected = voiceBridge_.IsVoiceDetected();

        // UI 更新
        UpdateVolumeUI(smoothedVolume);

        // 音声検出イベント
        if (bVoiceDetected) {
            OnVoiceInput();
        }
    }

    // クリーンアップ（EndPlay 相当）
    void Shutdown() {
        voiceBridge_.Stop();
        voiceBridge_.Shutdown();
        bIsInitialized = false;
    }

    // ゲッター
    float GetCurrentVolume() const {
        return voiceBridge_.GetCurrentVolume();
    }

    float GetSmoothedVolume() const {
        return voiceBridge_.GetSmoothedVolume();
    }

    bool IsVoiceDetected() const {
        return voiceBridge_.IsVoiceDetected();
    }

private:
    void UpdateVolumeUI(float volume) {
        // UI メーター更新
        if (VolumeBarWidget) {
            VolumeBarWidget->SetPercent(volume);
        }
    }

    void OnVoiceInput() {
        // 音声検出時の処理
        UE_LOG(LogAudio, Log, TEXT("Voice input detected"));
    }
};
```

---

### 5.5 マルチスレッド例（分離された音声処理）

```cpp
#include "MagVoiceBridge.h"
#include <thread>
#include <atomic>
#include <mutex>

class AudioProcessingThread {
private:
    MagVoiceBridge voiceBridge_;
    std::thread audioThread_;
    std::atomic<bool> bRunning{false};
    std::mutex statsLock_;
    MagVoiceBridge::VolumeStats lastStats_;

public:
    bool Start() {
        if (!voiceBridge_.Initialize() || !voiceBridge_.Start()) {
            return false;
        }

        bRunning = true;
        audioThread_ = std::thread(&AudioProcessingThread::ProcessAudio, this);
        return true;
    }

    void Stop() {
        bRunning = false;
        if (audioThread_.joinable()) {
            audioThread_.join();
        }
        voiceBridge_.Stop();
        voiceBridge_.Shutdown();
    }

    MagVoiceBridge::VolumeStats GetStats() const {
        std::lock_guard<std::mutex> lock(statsLock_);
        return lastStats_;
    }

private:
    void ProcessAudio() {
        while (bRunning) {
            voiceBridge_.Update();

            // 統計情報更新
            {
                std::lock_guard<std::mutex> lock(statsLock_);
                lastStats_ = voiceBridge_.GetVolumeStats();
            }

            // 少し待機（CPU 負荷軽減）
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

int main() {
    AudioProcessingThread audioThread;

    if (!audioThread.Start()) {
        std::cerr << "Failed to start audio thread\n";
        return 1;
    }

    // メインスレッドでは統計情報を読み込み
    for (int i = 0; i < 100; ++i) {
        auto stats = audioThread.GetStats();
        printf("Volume: %.2f, Voice: %s\n",
               stats.smoothedRMS, stats.isVoiceDetected ? "YES" : "NO");

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    audioThread.Stop();
    return 0;
}
```

---

## 6. よくある質問

### Q1: 初期化に失敗します

**A1: マイクが接続されているか確認してください**

```cpp
if (!voiceBridge.Initialize()) {
    // マイクの確認
    // 1. Windows 設定 → サウンド → 入力デバイス確認
    // 2. マイクドライバ確認
    // 3. 他のアプリケーションが占有していないか確認
}
```

---

### Q2: Update() を呼び出してもデータが取得できません

**A2: Start() を先に呼び出してください**

```cpp
voiceBridge.Initialize();  // これが必須
voiceBridge.Start();       // これも必須
voiceBridge.Update();      // その後で呼び出し
```

---

### Q3: 音量がずっと 0 です

**A3: マイク自体の音量を確認してください**

```
Windows 設定 → サウンド → 入力デバイス → 音量スライダー確認
```

また、GetVolumeRange() のパラメータをマイク感度に合わせて調整してください：

```cpp
voiceBridge.SetVolumeRange(-70.0f, -3.0f);  // より広い範囲
```

---

### Q4: 音声検出が常に false です

**A4: 閾値を調整してください**

```cpp
// より緩い判定
voiceBridge.SetVoiceDetectionThresholds(0.35f, -50.0f);

// または
voiceBridge.SetNoiseFloor(-60.0f);  // ノイズフロアを下げる
```

---

### Q5: CPU 使用率が高いです

**A5: Update() の呼び出し頻度を下げるか、別スレッド化してください**

```cpp
// 方法1: 呼び出し頻度を下げる
static int updateCounter = 0;
if (updateCounter++ % 3 == 0) {  // 3フレームごと
    voiceBridge.Update();
}

// 方法2: 別スレッド化（推奨）
std::thread audioThread([&]() {
    while (isRunning) {
        voiceBridge.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
});
```

---

### Q6: 複数マイクから入力したい

**A6: 現在のバージョンではデフォルトマイクのみに対応しています**

各マイクに対して別々の `MagVoiceBridge` インスタンスを作成することで対応可能です（将来の機能拡張で対応予定）。

---

### Q7: GetSamples() は重い処理ですか？

**A7: はい、内部でコピーが発生します**

使用頻度を制限してください：

```cpp
// 毎フレーム呼び出すのは避ける
if (frameCount++ % 10 == 0) {  // 10フレームごと
    auto samples = voiceBridge.GetSamples();
    // 波形表示等の処理
}
```

---

### Q8: スムージング係数をどう決めたらいいですか？

**A8: 用途に応じて調整してください**

```cpp
// UI メーター（安定性重視）
voiceBridge.SetSmoothingFactor(0.6f);

// リアルタイムグラフ（反応性重視）
voiceBridge.SetSmoothingFactor(0.2f);

// バランス型（デフォルト）
voiceBridge.SetSmoothingFactor(0.4f);
```

---

### Q9: dB 値の計算がおかしいです

**A9: dB は対数スケールです**

| 線形値 | dB |
|--------|-----|
| 0.1 | -20 dB |
| 0.01 | -40 dB |
| 0.001 | -60 dB |
| 0.0001 | -80 dB |

---

### Q10: ゼロクロス率（ZCR）って何ですか？

**A10: 信号が 0 を横切る回数の比率です**

- 低 ZCR (0.05～0.20): 低周波（人の声）
- 高 ZCR (0.40～0.60): 高周波（ノイズ）

---

## 7. トラブルシューティング

### トラブル: 音が小さすぎて取得できない

**原因**:
- マイク感度が低い
- 音量範囲の設定が合っていない

**解決策**:

```cpp
// 1. Windows の入力デバイス設定を確認
//    設定 → サウンド → 詳細オプション → 音量ゲイン確認

// 2. 音量範囲を拡大
voiceBridge.SetVolumeRange(-80.0f, -6.0f);  // より広い範囲

// 3. ノイズフロアを下げる
voiceBridge.SetNoiseFloor(-70.0f);
```

---

### トラブル: 背景ノイズが多く、音声検出がうまくいかない

**原因**:
- ノイズフロアが低すぎる
- ZCR 閾値が高すぎる

**解決策**:

```cpp
// 1. ノイズフロアを上げる
voiceBridge.SetNoiseFloor(-40.0f);

// 2. より厳密な音声判定
voiceBridge.SetVoiceDetectionThresholds(0.20f, -35.0f);

// 3. 複数フレームの判定結果を統合
bool voiceStable = true;
for (int i = 0; i < 5; ++i) {
    voiceBridge.Update();
    if (!voiceBridge.IsVoiceDetected()) {
        voiceStable = false;
        break;
    }
}
```

---

### トラブル: Update() 呼び出し後も古いデータが返される

**原因**:
- atomic 変数の読み込みタイミング
- スレッドセーフの問題ではなく、データ更新待ち

**解決策**:

```cpp
// 複数フレーム分、Update() を呼び出して安定化
for (int i = 0; i < 5; ++i) {
    voiceBridge.Update();
}

float stableVolume = voiceBridge.GetSmoothedVolume();
```

---

### トラブル: GetSamples() のサイズが大きすぎてメモリを圧迫

**原因**:
- 最大 10 秒分のサンプルが保存されている
- 高頻度で GetSamples() を呼び出している

**解決策**:

```cpp
// 1. ClearSamples() で定期的にクリア
if (frameCount++ % 60 == 0) {  // 60フレームごと
    voiceBridge.ClearSamples();
}

// 2. GetSamples() の呼び出し頻度を下げる
if (frameCount++ % 30 == 0) {  // 30フレームごと
    auto samples = voiceBridge.GetSamples();
}
```

---

### トラブル: 複数スレッドからのアクセスで不安定

**原因**:
- 複数スレッドから `Update()` を呼び出している
- `Update()` は単一スレッドから呼び出し、データ読み込みは複数スレッドから可能

**解決策**:

```cpp
// 推奨: Update() を単一スレッドで実行
std::thread audioThread([&]() {
    while (isRunning) {
        voiceBridge.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
});

// メインスレッドでは読み込みのみ
float volume = voiceBridge.GetSmoothedVolume();  // OK
bool voice = voiceBridge.IsVoiceDetected();      // OK
```

---

### トラブル: デストラクタで警告やエラーが発生

**原因**:
- Shutdown() を呼び出してない
- デストラクタが Shutdown() を呼び出しているが、エラー状態

**解決策**:

```cpp
// 正しい順序
{
    MagVoiceBridge voiceBridge;
    voiceBridge.Initialize();
    voiceBridge.Start();
    
    // 処理...
    
    voiceBridge.Stop();
    voiceBridge.Shutdown();
}  // デストラクタ呼び出し（安全）

// または RAII パターン
class AudioGuard {
    MagVoiceBridge& voiceBridge_;
public:
    AudioGuard(MagVoiceBridge& vb) : voiceBridge_(vb) {
        voiceBridge_.Initialize();
        voiceBridge_.Start();
    }
    ~AudioGuard() {
        voiceBridge_.Stop();
        voiceBridge_.Shutdown();
    }
};
```

---

### トラブル: 特定の環境でのみ動作しない

**原因**:
- マイクドライバの問題
- Windows サウンドシステムの設定
- WASAPI の互換性

**解決策**:

```cpp
// エラー情報をログ出力
bool MagVoiceBridge::Initialize() {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(result)) {
        printf("CoInitializeEx failed: 0x%X\n", result);
        return false;
    }
    // ...以下続く
}

// 結果のコード値を記録して報告
```

---

## まとめ

### チェックリスト

- [ ] ヘッダー・CPP ファイルをプロジェクトに追加
- [ ] Visual Studio でリンカー設定（Ole32.lib, Uuid.lib）
- [ ] Initialize() → Start() → Update（毎フレーム） → Stop() → Shutdown()
- [ ] 基本的な使用方法を実装
- [ ] パラメータを環境に合わせて調整
- [ ] マルチスレッド対応（必要に応じて）

### 推奨されるセットアップ

```cpp
// 1. グローバルまたはマネージャークラス内に保持
std::unique_ptr<MagVoiceBridge> g_voiceBridge;

// 2. エンジン初期化時
g_voiceBridge = std::make_unique<MagVoiceBridge>();
if (g_voiceBridge->Initialize() && g_voiceBridge->Start()) {
    // 成功
}

// 3. メインループ
g_voiceBridge->Update();

// 4. エンジン終了時
g_voiceBridge->Stop();
g_voiceBridge->Shutdown();
g_voiceBridge.reset();
```

---

**ドキュメント作成日**: 2026-05-04  
**対応バージョン**: MagVoiceBridge v1.00  
**最終更新**: 2026-05-04

---

## 参考資料

- [WASAPI (Windows Audio Session API) - Microsoft Docs](https://docs.microsoft.com/en-us/windows/win32/coreaudio/wasapi)
- [C++ std::atomic - cppreference](https://en.cppreference.com/w/cpp/atomic/atomic)
- [Audio Processing Fundamentals](https://www.dspguide.com/)

---

**ご不明な点やご要望があれば、お気軽にお問い合わせください。**
