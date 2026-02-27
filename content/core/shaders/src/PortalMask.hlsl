struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut VsMain(uint vid : SV_VertexID) {
    VSOut o;
    if (vid == 0) {
        o.pos = float4(-1.0, 1.0, 0.0, 1.0);
        o.uv  = float2(0.0, 0.0);
    } else if (vid == 1) {
        o.pos = float4(3.0, 1.0, 0.0, 1.0);
        o.uv  = float2(2.0, 0.0);
    } else {
        o.pos = float4(-1.0, -3.0, 0.0, 1.0);
        o.uv  = float2(0.0, 2.0);
    }
    return o;
}

float4 PsMain(VSOut i) : SV_TARGET {
    float2 uv = i.uv * 0.5f;
    float2 c = uv - float2(0.5f, 0.5f);
    float r2 = dot(c, c);
    clip(0.16f - r2); // 半径0.4の円形マスク
    return float4(0.0, 0.0, 0.0, 1.0);
}
