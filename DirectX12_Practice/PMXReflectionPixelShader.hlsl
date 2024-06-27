#include "PmxShaderHeader.hlsli"

float4 PS(Output input) : SV_TARGET
{
	float3 light = normalize(lightVec);

	float diffuseB = saturate(dot(-light, input.normal));
	float4 toonDif = toon.Sample(smpToon, float2(0, 1.0 - diffuseB));

	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	float4 color = tex.Sample(smp, input.uv);
	float alpha = color.w;

	float4 result = max(saturate(
		toonDif * diffuse * color + float4(specularB * specular.rgb, 1))
		, float4(color * ambient, 1));

	return float4(result.rgb, alpha);
}