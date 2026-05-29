#pragma once

#include <vector>

#include "core/math/Vec3.h"
#include "core/math/Vec4.h"

// ===============================================
// Particle
// 1つのパーティクル（粒子）が持つ「実行時の状態」を表す構造体
// ・EmitterInstance がこの配列を持つ
// ・Update() で毎フレーム更新される
// ===============================================
struct Particle {
    Vec3 position{};      // 現在位置
    Vec3 velocity{};      // 毎フレーム加算される速度

    Vec3 rotation{};      // 現在の回転
    Vec3 rotationSpeed{}; // 回転速度

    Vec3 scale{ 1,1,1 };       // 現在スケール
    Vec3 scaleSpeed{};         // スケール変化量
    Vec3 initialScale{ 1,1,1 }; // カーブ適用前の基準スケール

    Vec4 color{ 1,1,1,1 };       // 現在色
    Vec4 initialColor{ 1,1,1,1 }; // カーブ適用前の基準色

    float life = 0.0f;       // 生存している時間
    float maxLife = 1.0f;    // 寿命（これを超えたら死ぬ）
    bool  active = false;    // 生きているかどうか

    // --- トレイル（TrailModule が使用。無効時は空のままコストゼロ） ---
    std::vector<Vec3> trailPoints; // 過去位置の履歴（古い→新しい）
    float             trailTimer = 0.0f; // 次の履歴を記録するまでの経過時間
};
