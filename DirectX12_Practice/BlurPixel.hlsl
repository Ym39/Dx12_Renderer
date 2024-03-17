#include "HeaderPostprocess.hlsli"
#include "HeaderScreenForward.hlsli"

BlurOutput ps(Output input) : SV_Target
{
	float w, h, levels;
	tex.GetDimensions(0, w, h, levels);
	float dx = 1.0 / w;
	float dy = 1.0 / h;

	BlurOutput result;

	result.highLum = Get5x5GaussianBlur(texHighLum, smp, input.uv, dx, dy, float4(0, 0, 1, 1));
	result.color = Get5x5GaussianBlur(tex, smp, input.uv, dx, dy, float4(0, 0, 1, 1));

	return result;
};
