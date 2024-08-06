#include "SSRHeader.hlsli"

float2 PosToScreen(float3 pos)
{
	float4 clipPos = mul(proj, float4(pos, 1.0));
	return clipPos.xy / clipPos.w * 0.5 + 0.5;
}

float3 ScreenToView(float2 screenPos, float depth)
{
	float4 clipPos = float4(screenPos * 2.0 - 1.0, depth, 1.0);
	float4 viewPos = mul(invproj, clipPos);
	return viewPos.xyz / viewPos.w;
}

float3 RayMarching(float3 position, float3 direction)
{
	const int maxSteps = 50;
	const float maxDistance = 1000.0;
	float2 screenPos;

	[unroll(maxSteps)]
	for (int i = 0; i < maxSteps; ++i)
	{
		position += direction * ssrStepSize;
		screenPos = PosToScreen(position);

		if (screenPos.x < 0 || screenPos.x > 1 || 
			screenPos.y < 0 || screenPos.y > 1) 
		{
			break;
		}

		float detectDepth = texDepth.Sample(smp, screenPos);
		float4 viewPos = mul(invproj, float4(screenPos * float2(2, -2) + float2(-1, 1), detectDepth, 1));
		viewPos.xyz = viewPos.xyz / viewPos.w;

		if (viewPos.z > position.z)
		{
			return texColor.Sample(smp, screenPos).rgb;
		}

		if (length(position) > maxDistance)
		{
			break;
		}
	}

	return float3(0, 0, 0);
}

float4 main(Output input) : SV_TARGET
{
	float4 ssrMask = texSSRMask.Sample(smp, input.uv);

	if (ssrMask.r != 1.0f)
	{
		return float4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	float4 planerReflectionColor = texPlanerReflection.Sample(smp, input.uv);
	float luminance = dot(planerReflectionColor.rgb, float3(0.299, 0.587, 0.114));
	if (luminance > 0.0f)
	{
		return planerReflectionColor;
	}

	float depth = texDepth.Sample(smp, input.uv);

	float4 viewPos = mul(invproj, float4(input.uv * float2(2, -2) + float2(-1, 1), depth, 1));
	viewPos.xyz = viewPos.xyz / viewPos.w;
	float3 viewDir = normalize(viewPos);

	float3 normal = normalize((texNormal.Sample(smp, input.uv).xyz * 2) - 1);
	float3 reflectionDir = normalize(reflect(viewDir, normal));

	float3 reflectionColor = RayMarching(viewPos, reflectionDir);

	return float4(reflectionColor, 1.0f);
}