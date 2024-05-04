#include "FBXShaderHeader.hlsli"

float4 PS(Output input) : SV_TARGET
{
	float3 color = float3(1.0f, 1.0f, 1.0f);
	float3 diffuse = float3(0.1f, 0.1f, 0.1f);

	float3 light = normalize(lightVec);
	float nDotL = saturate(dot(-light, input.normal));
	//nDotL = nDotL * 0.5f + 0.5f;

	color *= nDotL;
	color += diffuse;

	return float4(color, 1.0f);
}