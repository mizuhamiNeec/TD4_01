Texture2D    gTex : register(t0);
SamplerState gSampler : register(s0);

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut VsMain(uint vid : SV_VertexID) {
    VSOut o;
    if (vid == 0) {
        o.pos = float4(-1.0, 1.0, 0.0, 1.0);
        o.uv = float2(0.0, 0.0);
    } else if (vid == 1) {
        o.pos = float4(3.0, 1.0, 0.0, 1.0);
        o.uv = float2(2.0, 0.0);
    } else {
        o.pos = float4(-1.0, -3.0, 0.0, 1.0);
        o.uv = float2(0.0, 2.0);
    }
    return o;
}

float4 PsMain(VSOut i) : SV_TARGET {
    float2 uv = i.uv;
    float4 col = gTex.Sample(gSampler, uv);

    float2 d = uv - float2(0.5, 0.5);
    float vignette = saturate(1.0 - dot(d, d) * 1.6);
    col.rgb *= vignette;
    return col;
}
