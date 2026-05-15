#pragma once
#include "Transform.h"
#include "MathFunctions.h"

// ===============================================
// Particle
// 1つのパーティクル（粒子）が持つ「実行時の状態」を表す構造体
// ・EmitterInstance がこの配列を持つ
// ・Update() で毎フレーム更新される
// ===============================================
struct Particle {
    Vector3 position{};      // 現在位置
    Vector3 velocity{};      // 毎フレーム加算される速度

    Vector3 rotation{};      // 現在の回転
    Vector3 rotationSpeed{}; // 回転速度

    Vector3 scale{ 1,1,1 };       // 現在スケール
    Vector3 scaleSpeed{};         // スケール変化量
    Vector3 initialScale{ 1,1,1 }; // カーブ適用前の基準スケール

    Vector4 color{ 1,1,1,1 };       // 現在色
    Vector4 initialColor{ 1,1,1,1 }; // カーブ適用前の基準色

    float life = 0.0f;       // 生存している時間
    float maxLife = 1.0f;    // 寿命（これを超えたら死ぬ）
    bool  active = false;    // 生きているかどうか
};
