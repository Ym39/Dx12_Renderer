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

	void SetScale(float x, float y, float z);
	const DirectX::XMFLOAT3& GetScale() const;

	void AddTranslation(float x, float y, float z);

	DirectX::XMFLOAT3 GetForward() const;
	DirectX::XMFLOAT3 GetRight() const;
	DirectX::XMFLOAT3 GetUp() const;

	DirectX::XMMATRIX& GetTransformMatrix() const;
	DirectX::XMMATRIX& GetViewMatrix() const;

private:
	DirectX::XMFLOAT3 mPosition;
	DirectX::XMFLOAT3 mRotation;
	DirectX::XMFLOAT3 mScale;
};

