#include "peraHeader.hlsli"

float4 Get5x5GaussianBlur(Texture2D<float4> tex, SamplerState smp, float2 uv, float dx, float dy, float4 rect) {
	float4 ret = tex.Sample(smp, uv);

	float l1 = -dx, l2 = -2 * dx;
	float r1 = dx, r2 = 2 * dx;
	float u1 = -dy, u2 = -2 * dy;
	float d1 = dy, d2 = 2 * dy;
	l1 = max(uv.x + l1, rect.x) - uv.x;
	l2 = max(uv.x + l2, rect.x) - uv.x;
	r1 = min(uv.x + r1, rect.z - dx) - uv.x;
	r2 = min(uv.x + r2, rect.z - dx) - uv.x;

	u1 = max(uv.y + u1, rect.y) - uv.y;
	u2 = max(uv.y + u2, rect.y) - uv.y;
	d1 = min(uv.y + d1, rect.w - dy) - uv.y;
	d2 = min(uv.y + d2, rect.w - dy) - uv.y;

	return float4((
		tex.Sample(smp, uv + float2(l2, u2)).rgb
		+ tex.Sample(smp, uv + float2(l1, u2)).rgb * 4
		+ tex.Sample(smp, uv + float2(0, u2)).rgb * 6
		+ tex.Sample(smp, uv + float2(r1, u2)).rgb * 4
		+ tex.Sample(smp, uv + float2(r2, u2)).rgb

		+ tex.Sample(smp, uv + float2(l2, u1)).rgb * 4
		+ tex.Sample(smp, uv + float2(l1, u1)).rgb * 16
		+ tex.Sample(smp, uv + float2(0, u1)).rgb * 24
		+ tex.Sample(smp, uv + float2(r1, u1)).rgb * 16
		+ tex.Sample(smp, uv + float2(r2, u1)).rgb * 4

		+ tex.Sample(smp, uv + float2(l2, 0)).rgb * 6
		+ tex.Sample(smp, uv + float2(l1, 0)).rgb * 24
		+ ret.rgb * 36
		+ tex.Sample(smp, uv + float2(r1, 0)).rgb * 24
		+ tex.Sample(smp, uv + float2(r2, 0)).rgb * 6

		+ tex.Sample(smp, uv + float2(l2, d1)).rgb * 4
		+ tex.Sample(smp, uv + float2(l1, d1)).rgb * 16
		+ tex.Sample(smp, uv + float2(0, d1)).rgb * 24
		+ tex.Sample(smp, uv + float2(r1, d1)).rgb * 16
		+ tex.Sample(smp, uv + float2(r2, d1)).rgb * 4

		+ tex.Sample(smp, uv + float2(l2, d2)).rgb
		+ tex.Sample(smp, uv + float2(l1, d2)).rgb * 4
		+ tex.Sample(smp, uv + float2(0, d2)).rgb * 6
		+ tex.Sample(smp, uv + float2(r1, d2)).rgb * 4
		+ tex.Sample(smp, uv + float2(r2, d2)).rgb
		) / 256.0f, ret.a);
}

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

	float4 highLum = texHighLum.Sample(smp, input.uv);

	return col;
	//return col;
	//return float4(ret.rgb, col.a);
}

float4 DeferrdPera1PS(Output input) : SV_TARGET
{
	float4 col = tex.Sample(smp, input.uv);

	if (input.uv.x < 0.2 && input.uv.y < 0.2)
	{
		float depth = depthTex.Sample(smp, input.uv * 5);
		depth = 1.0f - pow(depth, 30);
		return float4(depth, depth, depth, 1);
	}
	else if (input.uv.x < 0.2 && input.uv.y < 0.4)
	{
		return texNormal.Sample(smp, (input.uv - float2(0, 0.4)) * 5);
	}

	float4 normal = texNormal.Sample(smp, input.uv);
	float depthFromLight = normal.a;

	normal = normal * 2.0f - 1.0f;

	float3 light = normalize(float3(1.0f, -1.0f, 1.0f));
	const float ambient = 0.25f;
	float diffB = max(saturate(dot(normal.xyz, -light)), ambient);

	float shadowWeight = lerp(0.5f, 1.0f, depthFromLight);

	float4 resultColor = col * float4(diffB, diffB, diffB, 1);

	resultColor = float4(resultColor.rgb * shadowWeight, resultColor.a);

	return resultColor;
}

float4 VerticalBokehPS(Output input) : SV_TARGET
{
	float w, h, levels;
	tex.GetDimensions(0, w, h, levels);

	float dx = 1.0f / w;
	float dy = 1.0f / h;

	if (input.uv.x < 0.2 && input.uv.y >= 0.6 && input.uv.y < 0.8)
	{
		float s = texSSAO.Sample(smp, (input.uv - float2(0, 0.6)) * 5);
		return float4(s, s, s, 1);
	}

	//float4 ret = float4(0, 0, 0, 0);
	//float4 col = tex.Sample(smp, input.uv);

	//ret += bkweights[0] * col;
	//for (int i = 1; i < 8; ++i)
	//{
	//	ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0, dy * i));
	//	ret += bkweights[i >> 2][i % 4] * tex.Sample(smp, input.uv + float2(0, -dy * i));
	//}

	//float4 bloomAccum = float4(0, 0, 0, 0);
	//float2 uvSize = float2(1, 0.5);
	//float2 uvOfst = float2(0, 0);

	//for (int i = 0; i < 8; ++i)
	//{
	//	bloomAccum += Get5x5GaussianBlur(texShrinkHighLum, smp, input.uv * uvSize + uvOfst, dx, dy, float4(uvOfst, uvOfst + uvSize));
	//	uvOfst.y += uvSize.y;
	//	uvSize *= 0.5f;
	//}

	//return col + Get5x5GaussianBlur(texHighLum, smp, input.uv, dx, dy, float4(0, 0, 1, 1)) * saturate(bloomAccum);

	float depthDiff = abs(depthTex.Sample(smp, float2(0.5, 0.5)) - depthTex.Sample(smp, input.uv));

	depthDiff = pow(depthDiff, 0.5f);
	float2 uvSize = float2(1, 0.5);
	float2 uvOfst = float2(0, 0);

	float t = depthDiff * 8;
	float no;
	t = modf(t, no);

	float4 resultColor[2];

	resultColor[0] = tex.Sample(smp, input.uv);

	if (no == 0.0f)
	{
		resultColor[1] = Get5x5GaussianBlur(
		    texShrink, smp,
			input.uv * uvSize + uvOfst,
			dx, dy,
			float4(uvOfst, uvOfst + uvSize)
		);
	}
	else
	{
		for (int i = 1; i <= 8; ++i)
		{
			if (i - no < 0)
			{
				continue;
			}

			resultColor[i - no] = Get5x5GaussianBlur(
				texShrink, smp, input.uv * uvSize + uvOfst,
				dx, dy, float4(uvOfst, uvOfst + uvSize));

			uvOfst.y += uvSize.y;
			uvSize *= 0.5f;
			if (i - no > 1)
			{
				break;
			}
		}
	}

	return lerp(resultColor[1], resultColor[0], t);

	//return col;
	//return float4(ret.rgb, col.a);

	float dep = pow(depthTex.Sample(smp, input.uv), 20);
	return float4(dep, dep, dep, 1);
}

BlurOutput BlurPS(Output input) : SV_Target
{
	float w, h, levels;
	tex.GetDimensions(0, w, h, levels);
	float dx = 1.0 / w;
	float dy = 1.0 / h;

	BlurOutput result;

	result.color = Get5x5GaussianBlur(tex, smp, input.uv, dx, dy, float4(0, 0, 1, 1));
	result.highLum = Get5x5GaussianBlur(texHighLum, smp, input.uv, dx, dy, float4(0, 0, 1, 1));

	return result;
};
