struct Output
{
	float4 svpos : SV_POSITION;
	float4 pos : POSITION;
	float4 normal : NORMAL;
};

struct PixelOutput
{
	float4 color : SV_TARGET0;
	float4 normal : SV_TARGET1;
	float4 highLum : SV_TARGET2;
};

SamplerState smp : register(s0);

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
	float specularPower;
	float3 ambient;
	float padding;
};