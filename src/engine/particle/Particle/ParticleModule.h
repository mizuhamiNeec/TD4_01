#pragma once

class ParticleEmitterInstance;
struct Particle;

// ===============================================
// ParticleModule（抽象クラス）
// 将来、色変化・スケール変化・速度変化などを
// 「モジュール」として分離したいときのための基底クラス
// 
// 現時点（ステップ1）では、継承クラスはまだ作らず
// インターフェースだけ定義しておく
// ===============================================
class ParticleModule {
public:
    virtual ~ParticleModule() = default;

    // Emit 時の初期化処理
    virtual void ApplySpawn(ParticleEmitterInstance& emitter, Particle& p) {}

    // 更新処理（毎フレーム）
    virtual void ApplyUpdate(ParticleEmitterInstance& emitter, Particle& p, float dt) {}

    // 描画時の処理（必要なら）
    virtual void ApplyRender(ParticleEmitterInstance& emitter, Particle& p) {}
};
