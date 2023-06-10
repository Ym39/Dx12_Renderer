Texture2D<float4> tex : register(t0);
Texture2D<float4> texNormal : register(t1);
Texture2D<float4> texHighLum : register(t2);
Texture2D<float4> texShrinkHighLum : register(t3);
Texture2D<float4> depthTex : register(t4);
SamplerState smp : register(s0);

cbuffer Weight : register(b0)
{
	float4 bkweights[2];
};

struct Output 
{
	float4 svpos: SV_POSITION;
	float2 uv:TEXCOORD;
};