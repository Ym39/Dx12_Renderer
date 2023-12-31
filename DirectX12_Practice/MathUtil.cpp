#include "MathUtil.h"

namespace MathUtil
{
	float Clamp(float value, float min, float max)
	{
		if (value < min)
		{
			return min;
		}

		if (value > max)
		{
			return max;
		}

		return value;
	}

	XMFLOAT3 Clamp(XMFLOAT3& value, XMFLOAT3& min, XMFLOAT3& max)
	{
		return XMFLOAT3(
			Clamp(value.x, min.x, max.x),
			Clamp(value.y, min.y, max.y),
			Clamp(value.z, min.z, max.z));
	}

	XMFLOAT3 Clamp(XMFLOAT3 value, float& min, float& max)
	{
		return XMFLOAT3(
			Clamp(value.x, min, max),
			Clamp(value.y, min, max),
			Clamp(value.z, min, max));
	}

	XMFLOAT3 Sub(XMFLOAT3& a, XMFLOAT3& b)
	{
		return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
	}

	XMFLOAT3 Add(XMFLOAT3& a, XMFLOAT3& b)
	{
		return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
	}

	float Lerp(float a, float b, float t)
	{
		return a * (1 - t) + b * t;
	}

	XMFLOAT3 Lerp(XMFLOAT3 a, XMFLOAT3 b, float t)
	{
		return {Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t)};
	}
}
