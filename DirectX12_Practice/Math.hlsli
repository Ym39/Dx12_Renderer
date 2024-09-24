static const float PI = 3.14159265f;

float4x4 AngleAxis(float angle, float3 axis)
{
    float c, s;
    sincos(angle, s, c);

    float t = 1 - c;
    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    return float4x4(
        t * x * x + c, t * x * y - s * z, t * x * z + s * y, 0,
        t * x * y + s * z, t * y * y + c, t * y * z - s * x, 0,
        t * x * z - s * y, t * y * z + s * x, t * z * z + c, 0,
        0, 0, 0, 1
        );
}

float4x4 Translate(float3 position)
{
    return float4x4(
        1, 0, 0, position.x,
        0, 1, 0, position.y,
        0, 0, 1, position.z,
        0, 0, 0, 1
        );
}

float4x4 QuaternionToMatrix(float4 q)
{
    float x = q.x, y = q.y, z = q.z, w = q.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    return float4x4(
        1.0 - (yy + zz), xy + wz, xz - wy, 0.0,
        xy - wz, 1.0 - (xx + zz), yz + wx, 0.0,
        xz + wy, yz - wx, 1.0 - (xx + yy), 0.0,
        0.0, 0.0, 0.0, 1.0
        );
}

float4x4 ScaleMatrix(float3 scale)
{
    return float4x4(
        scale.x, 0, 0, 0,
        0, scale.y, 0, 0,
        0, 0, scale.z, 0,
        0, 0, 0, 1
        );
}

float RandomInstanceSeed(uint instanceID)
{
    return frac(sin(float(instanceID) * 12.9898) * 43758.5453);
}

float Random(float2 seed, float min, float max)
{
    float randomno = frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
    return lerp(min, max, randomno);
}

float NoiseRandom(float2 uv)
{
	return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float NoiseInterpolate(float a, float b, float t)
{
    return (1.0 - t) * a + (t * b);
}

float ValueNoise(float2 uv)
{
    float2 i = floor(uv);
    float2 f = frac(uv);
    f = f * f * (3.0 - 2.0 * f);

    uv = abs(frac(uv) - 0.5);
    float2 c0 = i + float2(0.0, 0.0);
    float2 c1 = i + float2(1.0, 0.0);
    float2 c2 = i + float2(0.0, 1.0);
    float2 c3 = i + float2(1.0, 1.0);
    float r0 = NoiseRandom(c0);
    float r1 = NoiseRandom(c1);
    float r2 = NoiseRandom(c2);
    float r3 = NoiseRandom(c3);

    float bottomOfGrid = NoiseInterpolate(r0, r1, f.x);
    float topOfGrid = NoiseInterpolate(r2, r3, f.x);
    float t = NoiseInterpolate(bottomOfGrid, topOfGrid, f.y);

    return t;
}


float SimpleNoise(float2 uv, float scale)
{
    float t = 0.0;

    float freq = pow(2.0, float(0));
    float amp = pow(0.5, float(3 - 0));
    t += ValueNoise(float2(uv.x * scale / freq, uv.y * scale / freq)) * amp;

    freq = pow(2.0, float(1));
    amp = pow(0.5, float(3 - 1));
    t += ValueNoise(float2(uv.x * scale / freq, uv.y * scale / freq)) * amp;

    freq = pow(2.0, float(2));
    amp = pow(0.5, float(3 - 2));
    t += ValueNoise(float2(uv.x * scale / freq, uv.y * scale / freq)) * amp;

    return t;
}