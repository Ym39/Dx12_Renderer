#include "FBXShaderHeader.hlsli"

Output VS(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD) 
{
	Output output;

	pos = mul(world, pos);

	output.pos = pos;
	output.svpos = mul(mul(proj, view), output.pos);
	output.normal = mul(world, normal);

	return output;
}