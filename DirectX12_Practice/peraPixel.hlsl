#include "peraHeader.hlsli"

float4 ps(Output input) : SV_TARGET
{
	/*float4 color = tex.Sample(smp, input.uv);
	float y = dot(color.rgb, float3(0.299, 0.587, 0.114));
	float4(color.rgb - fmod(color.rgb, 0.25f), color.a);

	float w, h, levels;
	tex.GetDimensions(0, w, h, levels);

	float dx = 1.0f / w;
	float dy = 1.0f / h;
	float4 ret = float4(0, 0, 0, 0);

	ret += tex.Sample(smp, input.uv + float2(-2 * dx, -2 * dy));
	ret += tex.Sample(smp, input.uv + float2(0, -2 * dy));
	ret += tex.Sample(smp, input.uv + float2(2 * dx, -2 * dy));

	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 0));
	ret += tex.Sample(smp, input.uv);
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 0));

	ret += tex.Sample(smp, input.uv + float2(-2 * dx, 2 * dy));
	ret += tex.Sample(smp, input.uv + float2(0, 2 * dy));
	ret += tex.Sample(smp, input.uv + float2(2 * dx, 2 * dy));

	return ret / 9.0f;*/

    float w, h, levels;
	tex.GetDimensions(0, w, h, levels);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	float4 ret = float4(0, 0, 0, 0);
	float4 col = tex.Sample(smp, input.uv);

	ret += bkweights[0] * col;
	for (int i = 1; i < 8; ++i) 
	{
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(i * dx, 0));
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(-i * dx, 0));
	}

	return col;
	//return float4(ret.rgb, col.a);
}

float4 VerticalBokehPS(Output input) : SV_TARGET
{
	float w, h, levels;
	tex.GetDimensions(0, w, h, levels);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	float4 ret = float4(0, 0, 0, 0);
	float4 col = tex.Sample(smp, input.uv);

	ret += bkweights[0] * col;
	for (int i = 1; i < 8; ++i)
	{
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0, dy * i));
		ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0, -dy * i));
	}

	return col;
	//return float4(ret.rgb, col.a);

	float dep = pow(depthTex.Sample(smp, input.uv), 20);
	return float4(dep, dep, dep, 1);
}