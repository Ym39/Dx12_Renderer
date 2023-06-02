Texture2D<float4> tex : register(t0);
Texture2D<float4> depthTex : register(t1);
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