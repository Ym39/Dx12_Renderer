struct InstanceData
{
	float3 position;
	float4 rotation;
	float3 scale;
	float4 color;
};

struct VertexOutput
{
	float4 svpos : SV_POSITION;
	float4 normal : NORMAL0;
	float2 uv : TEXCOORD;
	float4 color : COLOR;
};

struct PixelOutput
{
	float4 color : SV_TARGET0;
	float4 normal : SV_TARGET1;
	float4 highLum : SV_TARGET2;
};

StructuredBuffer<InstanceData> instanceBuffer : register(t0);

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

cbuffer GlobalParameterBuffer : register(b1)
{
	float time;
	float randomSeed;
	float2 padding;
}


