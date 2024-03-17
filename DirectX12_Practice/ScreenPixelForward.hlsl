#include "HeaderScreenForward.hlsli"
#include "HeaderPostprocess.hlsli"

float4 ps(Output input) : SV_TARGET
{
	if (input.uv.x < 0.2 && input.uv.y >= 0.6 && input.uv.y < 0.8)
	{
		float s = texBloomResult.Sample(smp, (input.uv - float2(0, 0.6)) * 5);
		return float4(s, s, s, 1);
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

	//float4 bloomAccum = float4(0, 0, 0, 0);
	//float2 uvSize = float2(1, 0.5);
	//float2 uvOffset = float2(0, 0);

	//for (int i = 0; i < 8; ++i)
	//{
	//	bloomAccum += Get5x5GaussianBlur(texShrinkHighLum, smp, input.uv * uvSize + uvOffset, dx, dy, float4(uvOffset, uvOffset + uvSize));
	//	uvOffset.y += uvSize.y;
	//	uvSize *= 0.5f;
	//}

	//float4 bloomColor = Get5x5GaussianBlur(texHighLum, smp, input.uv, dx, dy, float4(0, 0, 1, 1)) + saturate(bloomAccum);
	float4 bloomColor = texBloomResult.Sample(smp, input.uv);
	result += bloomColor;

#endif

	return result;
}
