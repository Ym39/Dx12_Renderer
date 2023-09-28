#include "BasicShaderHeader.hlsli"

float4 BasicPS(Output input) : SV_TARGET
{
	if (input.instNo == 1)
	{
		return float4(0, 0, 0, 1);
	}

	float3 light = normalize(lightVec);

	float diffuseB = saturate(dot(-light, input.normal));
	float4 toonDif = toon.Sample(smpToon, float2(0, 1.0 - diffuseB));

	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	float4 color = tex.Sample(smp, input.uv);
	float2 sphereMapUV = (input.vnormal.xy + float2(1, -1)) * float2(0.5, -0.5);

	float4 result = max(saturate(
		toonDif
		* diffuse
		* color
		* sph.Sample(smp, sphereMapUV)
		+ spa.Sample(smp, sphereMapUV)
		+ float4(specularB * specular.rgb, 1))
		, float4(color * ambient, 1));

	float3 posFromLightVP = input.tpos.xyz / input.tpos.w;
	float2 shadowUV = (posFromLightVP + float2(1, -1)) * float2(0.5, -0.5);
	float depthFromLight = lightDepthTex.SampleCmp(shadowSmp, shadowUV, posFromLightVP.z - 0.005f);

	float shadowWeight = lerp(0.5f, 1.0f, depthFromLight);

	result = float4(result.rgb * shadowWeight, result.a);

	return result;
}

PixelOutput DeferrdPS(Output input)
{
	PixelOutput output;

	float3 light = normalize(lightVec);

	float diffuseB = saturate(dot(-light, input.normal));
	float4 toonDif = toon.Sample(smpToon, float2(0, 1.0 - diffuseB));

	float3 refLight = normalize(reflect(light, input.normal.xyz));
	float specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);

	float4 color = tex.Sample(smp, input.uv);
	float2 sphereMapUV = (input.vnormal.xy + float2(1, -1)) * float2(0.5, -0.5);

	float4 result = max(saturate(
		toonDif
		* diffuse
		* color
		* sph.Sample(smp, sphereMapUV)
		+ spa.Sample(smp, sphereMapUV)
		+ float4(specularB * specular.rgb, 1))
		, float4(color * ambient, 1));


	float4 texColor = tex.Sample(smp, input.uv);

	float4 spaColor = spa.Sample(smp, sphereMapUV);
	float4 sphColor = sph.Sample(smp, sphereMapUV);

	float3 posFromLightVP = input.tpos.xyz / input.tpos.w;
    float2 shadowUV = (posFromLightVP + float2(1, -1)) * float2(0.5, -0.5);
    float depthFromLight = lightDepthTex.SampleCmp(shadowSmp, shadowUV, posFromLightVP.z - 0.005f);

	//output.color = float4(spaColor + sphColor * texColor * diffuse);
	output.color = result;
	output.normal.rgb = float3((input.normal.xyz + 1.0f) / 2.0f);
	output.normal.a = depthFromLight;

	float y = dot(float3(0.299f, 0.587f, 0.114f), output.color);
	output.highLum = y > 0.7f ? output.color : 0.0f;

	return output;
}