#include "BasicShaderHeader.hlsli"

Output BasicVS (
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	min16uint2 boneno : BONENO,
	min16uint weight : WEIGHT) 
{
	Output output;
	pos = mul(bones[boneno[0]], pos);
	pos = mul(world, pos);
    output.svpos = mul(mul(proj, view), pos);
	output.pos = mul(view, pos);
	normal.w = 0;
	output.normal = mul(world, normal);
	output.vnormal = mul(view, output.normal);
	output.uv = uv;
	output.ray = normalize(pos.xyz - mul(view, eye));
	return output;
}