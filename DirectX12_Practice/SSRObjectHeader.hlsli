struct VertexOutput
{
	float4 svpos : SV_POSITION;
	float4 normal : NORMAL0;
	float2 uv : TEXCOORD;
};

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

cbuffer Transform: register(b1)
{
	matrix world;
};