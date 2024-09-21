struct Output
{
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float3 normal : NORMAL;
	float4 tpos : TPOS;
};

struct PixelOutput
{
	float4 color : SV_TARGET0;
	float4 normal : SV_TARGET1;
	float4 highLum : SV_TARGET2;
};

Texture2D<float4> shadowDepthTexture: register(t0);
Texture2D<float4> reflectionRenderTexture: register(t1);

SamplerState smp : register(s0);
SamplerComparisonState shadowSmp : register(s1);

cbuffer SceneBuffer : register(b0)
{
	matrix view;
	matrix proj;
	matrix invproj;
	matrix lightCamera;
	matrix shadow;
	float4 lightVec;
	float3 eye;
};

cbuffer Transform : register(b1)
{
	matrix world;
};

cbuffer Material : register(b2)
{
	float4 diffuse;
	float3 specular;
	float roughness;
	float3 ambient;
	float bloomFactor;
};

cbuffer Resolution : register(b3)
{
	float screenWidth;
	float screenHeight;
};