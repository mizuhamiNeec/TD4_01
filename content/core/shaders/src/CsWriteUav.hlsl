RWTexture2D<float4> gOut : register(u0);

[numthreads(8,8,1)]
void main(uint3 dtid : SV_DispatchThreadID) {
	uint w, h;
	gOut.GetDimensions(w, h);

	float2 uv     = float2(dtid.xy) / float2(max(w, 1), max(h, 1));
	gOut[dtid.xy] = float4(uv.x, uv.y, 0.2, 1.0);
}
