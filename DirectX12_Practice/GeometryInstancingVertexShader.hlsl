#include "GeometryInstancingHeader.hlsli"
#include "Math.hlsli"

VertexOutput main(
	float4 pos : POSITION,
	float4 normal : NORMAL,
	float2 uv : TEXCOORD,
	uint instNo : SV_InstanceID)
{
	VertexOutput output;

	float4x4 t = Translate(instanceBuffer[instNo].position);
	float4x4 r = QuaternionToMatrix(instanceBuffer[instNo].rotation);
	float4x4 s = ScaleMatrix(instanceBuffer[instNo].scale);

	float seed = RandomInstanceSeed(instNo);
	float random = Random(float2(seed, seed), -100000, 100000);
	float noise = SimpleNoise(float2(random, time * 0.3f), 3.0f);

	float frequency = 0.6f;
	float angle = sin(2 * PI * frequency * time + noise);

	float3 zAxis = float3(r._31, r._32, r._33);
	float random2 = Random(float2(seed, seed), -100000, 100000);
	float noise2 = SimpleNoise(float2(random2, time * 0.3f), 3.0f);
	float random3 = Random(float2(seed, seed), -1, 1);
	zAxis = normalize(float3(random3 * noise2, 0, 1));

	angle *= Random(float2(seed, seed), 0.5f, 1.0f);

	float4x4 swingRotation = AngleAxis(angle, zAxis);

	r = mul(r, swingRotation);

	float4x4 world = mul(t, mul(r, s));

	float4x4 swingTranslate = Translate(float3(0.0, 2.0f, 0.0f));

	world = mul(world, swingTranslate);

	float4 worldPosition = mul(world, pos);
	output.color = instanceBuffer[instNo].color;
	output.svpos = mul(mul(proj, view), worldPosition);
	output.normal = mul(world, normal);
	output.uv = uv;

	return output;
}