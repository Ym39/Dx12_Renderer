#include "FBXShaderHeader.hlsli"

float4 PS(Output input) : SV_TARGET
{
	float3 light = normalize(lightVec);
	float nDotL = saturate(dot(-light, input.normal));

	float3 color = diffuse;

	color *= nDotL;
	color += ambient;

	return float4(color, 1.0f);
}