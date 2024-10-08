#pragma once
#include <DirectXMath.h>

class Transform
{
public:
	Transform();
	~Transform() = default;

	void SetPosition(float x, float y, float z);
	const DirectX::XMFLOAT3& GetPosition() const;

	void SetRotation(float x, float y, float z);
	const DirectX::XMFLOAT3& GetRotation() const;
	const DirectX::XMFLOAT3 GetRotationRadians() const;
	const DirectX::XMFLOAT4 GetQuaternion() const;

	void SetScale(float x, float y, float z);
	const DirectX::XMFLOAT3& GetScale() const;

	void AddTranslation(float x, float y, float z);

	DirectX::XMFLOAT3 GetForward() const;
	DirectX::XMFLOAT3 GetRight() const;
	DirectX::XMFLOAT3 GetUp() const;

	DirectX::XMMATRIX& GetTransformMatrix() const;
	DirectX::XMMATRIX& GetViewMatrix() const;

	DirectX::XMMATRIX& GetPlanarReflectionsViewMatrix(const DirectX::XMFLOAT3& planeNormal, const DirectX::XMFLOAT3& planePosition) const;
	DirectX::XMMATRIX& GetPlanarReflectionsTransform(const DirectX::XMFLOAT3& planeNormal, const DirectX::XMFLOAT3& planePosition) const;

private:
	DirectX::XMFLOAT3 mPosition;
	DirectX::XMFLOAT3 mRotation;
	DirectX::XMFLOAT3 mScale;
};

