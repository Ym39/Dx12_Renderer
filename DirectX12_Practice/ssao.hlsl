#include "peraHeader.hlsli"

float random(float2 uv)
{
	return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float SsaoPs(Output input) : SV_Target
{
	float depth = depthTex.Sample(smp, input.uv);

	float w, h, miplevels;

	depthTex.GetDimensions(0, w, h, miplevels);
	float dx = 1.0f / w;
	float dy = 1.0f / h;

	float4 respos = mul(invproj, float4(input.uv * float2(2, -2) + float2(-1, 1), depth, 1));

	respos.xyz = respos.xyz / respos.w;

	float div = 0.0f;
	float ao = 0.0f;
	float3 normal = normalize((texNormal.Sample(smp, input.uv).xyz * 2) - 1);
	const int trycnt = 32;
	const float radius = 0.5f;

	if (depth < 1.0f)
	{
		for (int i = 0; i < trycnt; ++i)
		{
			float rnd1 = random(float2(i * dx, i * dy)) * 2 - 1;
			float rnd2 = random(float2(rnd1, i * dy)) * 2 - 1;
			float rnd3 = random(float2(rnd2, rnd1)) * 2 - 1;
			float3 omega = normalize(float3(rnd1, rnd2, rnd3));
			omega = normalize(omega);

			float dt = dot(normal, omega);
			float sgn = sign(dt);
			omega *= sign(dt);

			float4 rpos = mul(proj, float4(respos.xyz + omega * radius, 1));
			rpos.xyz /= rpos.w;
			dt *= sgn;
			div += dt;

			ao += step(depthTex.Sample(smp, (rpos.xy + float2(1, -1)) * float2(0.5f, -0.5f)), rpos.z) * dt;
		}

		ao /= div;
	}

	return 1.0f;
}