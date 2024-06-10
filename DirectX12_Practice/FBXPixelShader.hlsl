#include "FBXShaderHeader.hlsli"
#include "BRDF.hlsli"

PixelOutput PS(Output input) : SV_TARGET
{
	float3 normal = normalize(input.normal);
	float3 view = normalize(eye - input.pos);
	float3 light = normalize(lightVec);
	float3 F0 = float3(0.04f, 0.04f, 0.04f);

	float3 specularColor = CookTorranceBRDF(normal, view, light, F0, roughness) * specular;

	float3 diffuseColor = diffuse / PI;
	float3 finalColor = diffuseColor + specularColor + ambient;

	PixelOutput output;

	output.color = float4(finalColor, 1.0f);
	output.highLum = diffuse * bloomFactor;
	output.normal.rgb = float3((input.normal.xyz + 1.0f) / 2.0f);
	output.normal.a = 1.0f;

	return output;
}