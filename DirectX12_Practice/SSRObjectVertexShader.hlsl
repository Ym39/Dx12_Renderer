#include "SSRObjectHeader.hlsli"

VertexOutput main(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD)
{
	VertexOutput output;

	output.svpos = mul(mul(proj, view), mul(world, pos));
	output.normal = mul(world, normal);
	output.uv = uv;

	return output;
}
