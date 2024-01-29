#pragma once
#include<DirectXMath.h>
#include <LinearMath/btTransform.h>

using namespace DirectX;

namespace MathUtil
{
	float Clamp(float value, float min, float max);

	XMFLOAT3 Clamp(XMFLOAT3& value, XMFLOAT3& min, XMFLOAT3& max);

	XMFLOAT3 Clamp(XMFLOAT3 value, float& min, float& max);

	XMFLOAT3 Sub(XMFLOAT3& a, XMFLOAT3& b);

	XMFLOAT3 Add(XMFLOAT3& a, XMFLOAT3& b);

	float Lerp(float a, float b, float t);

	XMFLOAT3 Lerp(XMFLOAT3 a, XMFLOAT3 b, float t);

	DirectX::XMMATRIX& GetMatrixFrombtTransform(btTransform& btTransform);

	void GetRowMajorMatrix(const XMMATRIX& matrix, float* out);
}
