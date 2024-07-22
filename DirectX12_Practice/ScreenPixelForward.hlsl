#include "HeaderScreenForward.hlsli"
#include "HeaderPostprocess.hlsli"

float4 ps(Output input) : SV_TARGET
{
	if (input.uv.x < 0.2 && input.uv.y >= 0.6 && input.uv.y < 0.8)
	{
		float3 texBloom = texBloomResult.Sample(smp, (input.uv - float2(0, 0.6)) * 5);
		return float4(texBloom, 1);
	}

	float w, h, levels;
	tex.GetDimensions(0, w, h, levels);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	float4 texColor = tex.Sample(smp, input.uv);
	float4 result = texColor;

#ifdef SSAO
	float4 ssao = texSSAO.Sample(smp, input.uv);
	result *= 1 - ssao;
#endif

#ifdef BLOOM
	float4 bloomColor = texBloomResult.Sample(smp, input.uv);
	result += bloomColor;
#endif

	return result;
}
