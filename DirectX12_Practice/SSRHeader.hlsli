Texture2D<float4> texColor : register(t0);
Texture2D<float4> texNormal : register(t1);
Texture2D<float4> texSSRMask : register(t2);
Texture2D<float4> texPlanerReflection : register(t3);
Texture2D<float> texDepth : register(t4);
SamplerState smp : register(s0);

cbuffer SceneBuffer : register(b0)
{
	matrix view;
	matrix proj;
	matrix invproj;
	matrix lightCamera;
	matrix shadow;
	float3 eye;
};

cbuffer PostProcessParameter : register(b1)
{
	int iteration;
	float intensity;
	float ssrStepSize;
};

cbuffer Resolution : register(b2)
{
	float screenWidth;
	float screenHeight;
};

struct Output
{
	float4 svpos: SV_POSITION;
	float2 uv:TEXCOORD;
};