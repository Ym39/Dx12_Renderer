#include "PmxShaderHeader.hlsli"

float4 PS(Output input) : SV_TARGET
{
	float3 light = normalize(float3(1, -1, 1));

	float diffuseB = saturate(dot(-light, input.normal));
	float4 toonDif = toon.Sample(smpToon, float2(0, 1.0 - diffuseB));

	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	float4 color = tex.Sample(smp, input.uv);
	float2 sphereMapUV = (input.vnormal.xy + float2(1, -1)) * float2(0.5, -0.5);

	return max(saturate(
		toonDif
		* diffuse
		* color
		+ float4(specularB * specular.rgb, color.a))
		, float4(color * ambient, color.a));
}