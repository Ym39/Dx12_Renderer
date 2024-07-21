#include "GeometryInstancingHeader.hlsli"

PixelOutput main(VertexOutput input) : SV_TARGET
{
	PixelOutput output;

	output.color = input.color;
	output.highLum = input.color;
	output.normal.rgb = float3((input.normal.xyz + 1.0f) / 2.0f);
	output.normal.a = 1.0f;

	return output;
}