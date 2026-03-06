#pragma once
#include <cstdint>

// ---- System / Window --------------------------------------------------------
constexpr uint32_t kIconPower    = 0xE8AC; // 電源
constexpr uint32_t kIconSettings = 0xE8B8; // 設定
constexpr uint32_t kIconReset    = 0xE166; // リセット
constexpr uint32_t kIconClose    = 0xE5CD; // 閉じる

// ---- Navigation / App -------------------------------------------------------
constexpr uint32_t kIconArrowBack = 0xE5C4; // 戻る/ロゴ(仮)

// ---- Help / Info ------------------------------------------------------------
constexpr uint32_t kIconInfo    = 0xE88E; // 情報
constexpr uint32_t kIconHelp    = 0xE887; // ヘルプ
constexpr uint32_t kIconAvgTime = 0xF813; // 平均時間 (プロファイラー用)

// ---- Console ----------------------------------------------------------------
constexpr uint32_t kIconTerminal = 0xEB8E; // ターミナル

// ---- Labels / UI elements ---------------------------------------------------
constexpr uint32_t kIconLabel     = 0xE892; // ラベル
constexpr uint32_t kIconRectangle = 0xEB54; // 四角形

// ---- File / IO --------------------------------------------------------------
constexpr uint32_t kIconSave   = 0xE161; // セーブ
constexpr uint32_t kIconSaveAs = 0xEB60; // 名前を付けて保存

constexpr uint32_t kIconCopy     = 0xE14D; // コピー
constexpr uint32_t kIconDownload = 0xF090; // インポート
constexpr uint32_t kIconUpload   = 0xF09B; // エクスポート

// ---- Visibility -------------------------------------------------------------
constexpr uint32_t kIconVisibility    = 0xE8F4; // On
constexpr uint32_t kIconVisibilityOff = 0xE8F5; // Off

// ---- CheckBox ---------------------------------------------------------------
constexpr uint32_t kIconCheckBoxOn  = 0xE834; // チェックボックスオン
constexpr uint32_t kIconCheckBoxOff = 0xE835; // チェックボックスオフ

// ---- Playback / Time --------------------------------------------------------
constexpr uint32_t kIconPlay  = 0xE037; // プレイ
constexpr uint32_t kIconPause = 0xE034; // ポーズ
constexpr uint32_t kIconStop  = 0xE047; // ストップ
constexpr uint32_t kIconTimer = 0xE425; // タイマー
constexpr uint32_t kIconSpeed = 0xE9E4; // スピード

// ---- Scene / Data types -----------------------------------------------------
constexpr uint32_t kIconEntity = 0xEA24; // エンティティ
constexpr uint32_t kIconObject = 0xE3B4; // オブジェクト
constexpr uint32_t kIconGroup  = 0xE574; // グループ
constexpr uint32_t kIconMesh   = 0xE3EC; // メッシュ
constexpr uint32_t kIconVertex = 0xEBC7; // 頂点
constexpr uint32_t kIconEdge   = 0xE922; // エッジ
constexpr uint32_t kIconFace   = 0xE86B; // 面

// ---- Gizmo / Transform ------------------------------------------------------
constexpr uint32_t kIconSelect = 0xF82F; // 選択
constexpr uint32_t kIconMove   = 0xF71E; // 移動
constexpr uint32_t kIconRotate = 0xE042; // 回転
constexpr uint32_t kIconScale  = 0xF707; // 拡縮
constexpr uint32_t kIconPivot  = 0xE147; // 原点

// ---- Primitives / Assets ----------------------------------------------------
constexpr uint32_t kIconBox     = 0xF720; // ボックス
constexpr uint32_t kIconTexture = 0xE421; // テクスチャ

// ---- UI helpers -------------------------------------------------------------
constexpr uint32_t kIconFilter   = 0xEF4F; // フィルター
constexpr uint32_t kIconDropDown = 0xE5C5; // ドロップダウン
