#include "HeaderPostprocess.hlsli"

Texture2D<float4> texHighLum : register(t0);
Texture2D<float4> texShrinkBlur : register(t1);
SamplerState smp : register(s0);

cbuffer BloomParameter : register(b0)
{
	int iteration;
	float intensity;
	float2 padding;
};

struct Output
{
	float4 svpos: SV_POSITION;
	float2 uv:TEXCOORD;
};

float4 ps(Output input) : SV_TARGET
{
	float w, h, levels;
	texHighLum.GetDimensions(0, w, h, levels);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	float4 bloomAccum = float4(0, 0, 0, 0);
	float2 uvSize = float2(1, 0.5);
	float2 uvOffset = float2(0, 0);

	for (int i = 0; i < iteration - 1; ++i)
	{
		bloomAccum += Get5x5GaussianBlur(texShrinkBlur, smp, input.uv * uvSize + uvOffset, dx, dy, float4(uvOffset, uvOffset + uvSize));
		uvOffset.y += uvSize.y;
		uvSize *= 0.5f;
	}

	float4 bloomColor = Get5x5GaussianBlur(texHighLum, smp, input.uv, dx, dy, float4(0, 0, 1, 1)) * intensity + saturate(bloomAccum);
	return bloomColor;
}