#ifndef POST_FX_PARAMS_HLSLI
#define POST_FX_PARAMS_HLSLI

cbuffer PostFxParams : register(b0) {
	float4 gPostFxScalar0;
	float4 gPostFxScalar1;
	float4 gPostFxColor0;
	float4 gPostFxColor1;
}

#endif
