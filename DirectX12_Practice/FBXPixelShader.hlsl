#include "FBXShaderHeader.hlsli"
#include "BRDF.hlsli"

float4 BoxBlur(float2 uv)
{
	float2 screenSize = float2(screenWidth, screenHeight);
	float2 texelSize = 1.0 / screenSize;

	float4 color = float4(0.0, 0.0, 0.0, 0.0);
	for (int y = -2; y <= 2; ++y)
	{
		float2 offset = float2(0, y) * texelSize;
		color += reflectionRenderTexture.Sample(smp, uv + offset);
	}

	color /= 3.0f;
	return color;
}

PixelOutput PS(Output input) : SV_TARGET
{
	float3 normal = normalize(input.normal);
	float3 view = normalize(eye - input.pos);
	float3 light = normalize(lightVec);
	float3 F0 = float3(0.04f, 0.04f, 0.04f);

	float3 specularColor = CookTorranceBRDF(normal, view, light, F0, roughness) * specular;

	float3 diffuseColor = diffuse / PI;
	float3 finalColor = diffuseColor + specularColor + ambient;

	float2 ndc;
	ndc.x = input.svpos.x / screenWidth;
	ndc.y = input.svpos.y / screenHeight;
	ndc.x = ndc.x * 2.0f - 1.0f;
	ndc.y = 1.0f - ndc.y * 2.0f;

	float2 renderTextureUV = ndc * 0.5f + 0.5f;
	renderTextureUV.y = 1.0f - renderTextureUV.y;
	float4 reflection = BoxBlur(renderTextureUV);

	PixelOutput output;
	output.color = float4(lerp(finalColor.rgb, reflection.rgb, 0.4), 1.0f);
	output.highLum = diffuse * bloomFactor;
	output.normal.rgb = float3((input.normal.xyz + 1.0f) / 2.0f);
	output.normal.a = 1.0f;

	return output;
}