struct Output
{
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float4 normal : NORMAL0;
	float4 vnormal : NORMAL1;
	float2 uv : TEXCOORD;
	float3 ray : VECTOR;
	uint instNo : SV_InstanceID;
	float4 tpos : TPOS;
};

struct PixelOutput
{
	float4 color : SV_TARGET0;
	float4 normal : SV_TARGET1;
	float4 highLum : SV_TARGET2;
};

Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
Texture2D<float4> toon : register(t3);
Texture2D<float4> lightDepthTex : register(t4);
Texture2D<float4> normalTex : register(t5);

SamplerState smp : register(s0);
SamplerState smpToon : register(s1);
SamplerComparisonState shadowSmp : register(s2);

cbuffer SceneBuffer : register(b0)
{
	matrix view;
	matrix proj;
	matrix invproj;
	matrix lightCamera;
	matrix shadow;
	float3 eye;
};

cbuffer Transform: register(b1)
{
	matrix world;
	matrix bones[256];
};

cbuffer Material : register(b2)
{
	float4 diffuse;
	float4 specular;
	float3 ambient;
};