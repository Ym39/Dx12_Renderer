#include "peraHeader.hlsli"

float4 ps(Output input) : SV_TARGET
{
	float4 color = tex.Sample(smp, input.uv);
	float y = dot(color.rgb, float3(0.299, 0.587, 0.114));
	float4(color.rgb - fmod(color.rgb, 0.25f), color.a);
	return color;
}