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

	DirectX::XMMATRIX& GetMatrixFrombtTransform(btTransform& btTransform)
	{
		float m[16];
		btTransform.getOpenGLMatrix(m);

		XMMATRIX resultMatrix;
		resultMatrix.r[0].m128_f32[0] = m[0];
		resultMatrix.r[1].m128_f32[0] = m[1];
		resultMatrix.r[2].m128_f32[0] = m[2];
		resultMatrix.r[3].m128_f32[0] = m[3];

		resultMatrix.r[0].m128_f32[1] = m[4];
		resultMatrix.r[1].m128_f32[1] = m[5];
		resultMatrix.r[2].m128_f32[1] = m[6];
		resultMatrix.r[3].m128_f32[1] = m[7];

		resultMatrix.r[0].m128_f32[2] = m[8];
		resultMatrix.r[1].m128_f32[2] = m[9];
		resultMatrix.r[2].m128_f32[2] = m[10];
		resultMatrix.r[3].m128_f32[2] = m[11];

		resultMatrix.r[0].m128_f32[3] = m[12];
		resultMatrix.r[1].m128_f32[3] = m[13];
		resultMatrix.r[2].m128_f32[3] = m[14];
		resultMatrix.r[3].m128_f32[3] = m[15];

		resultMatrix = XMMatrixTranspose(resultMatrix);

		return resultMatrix;
	}

	void GetRowMajorMatrix(const XMMATRIX& matrix, float* out)
	{
		XMMATRIX rowMajor = XMMatrixTranspose(matrix);

		out[0] = rowMajor.r[0].m128_f32[0];
		out[1] = rowMajor.r[1].m128_f32[0];
		out[2] = rowMajor.r[2].m128_f32[0];
		out[3] = rowMajor.r[3].m128_f32[0];
		out[4] = rowMajor.r[0].m128_f32[1];
		out[5] = rowMajor.r[1].m128_f32[1];
		out[6] = rowMajor.r[2].m128_f32[1];
		out[7] = rowMajor.r[3].m128_f32[1];
		out[8] = rowMajor.r[0].m128_f32[2];
		out[9] = rowMajor.r[1].m128_f32[2];
		out[10] = rowMajor.r[2].m128_f32[2];
		out[11] = rowMajor.r[3].m128_f32[2];
		out[12] = rowMajor.r[0].m128_f32[3];
		out[13] = rowMajor.r[1].m128_f32[3];
		out[14] = rowMajor.r[2].m128_f32[3];
		out[15] = rowMajor.r[3].m128_f32[3];
	}
}
