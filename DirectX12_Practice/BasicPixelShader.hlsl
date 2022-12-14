#include "BasicShaderHeader.hlsli"

float4 BasicPS(Output input) : SV_TARGET
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
		* sph.Sample(smp, sphereMapUV)
		+ spa.Sample(smp, sphereMapUV)
		+ float4(specularB * specular.rgb, 1))
        , float4(color * ambient, 1));
}