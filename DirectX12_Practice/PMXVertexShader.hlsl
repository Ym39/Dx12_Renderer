#include "PmxShaderHeader.hlsli"

Output VS(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	uint instNo : SV_InstanceID)
{
	Output output;

	pos = mul(world, pos);

	output.pos = pos;
	output.svpos = mul(mul(proj, view), output.pos);
	output.tpos = mul(lightCamera, output.pos);
	normal.w = 0;
	output.normal = mul(world, normal);
	output.vnormal = mul(view, output.normal);
	output.uv = uv;
	output.ray = normalize(pos.xyz - mul(view, eye));
	output.instNo = instNo;

	return output;
}

float4 ShadowVS(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	uint instNo : SV_InstanceID) : SV_POSITION
{
	pos = mul(world, pos);

	return mul(lightCamera, pos);
}


