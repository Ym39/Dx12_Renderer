#pragma once
#include<DirectXMath.h>

using namespace DirectX;

namespace MathUtil
{
	float Clamp(float value, float min, float max);

	XMFLOAT3 Clamp(XMFLOAT3& value, XMFLOAT3& min, XMFLOAT3& max);

	XMFLOAT3 Clamp(XMFLOAT3 value, float& min, float& max);

	XMFLOAT3 Sub(XMFLOAT3& a, XMFLOAT3& b);

	XMFLOAT3 Add(XMFLOAT3& a, XMFLOAT3& b);
}
