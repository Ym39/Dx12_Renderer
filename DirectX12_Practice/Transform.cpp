#include "Transform.h"

Transform::Transform():
	mPosition(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f)),
	mRotation(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f)),
	mScale(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f))
{
}

void Transform::SetPosition(float x, float y, float z)
{
	mPosition.x = x;
	mPosition.y = y;
	mPosition.z = z;
}

const DirectX::XMFLOAT3& Transform::GetPosition() const
{
	return mPosition;
}

void Transform::SetRotation(float x, float y, float z)
{
	mRotation.x = DirectX::XMConvertToRadians(x);
	mRotation.y = DirectX::XMConvertToRadians(y);
	mRotation.z = DirectX::XMConvertToRadians(z);
}

const DirectX::XMFLOAT3& Transform::GetRotation() const
{
	return DirectX::XMFLOAT3(DirectX::XMConvertToDegrees(mRotation.x), DirectX::XMConvertToDegrees(mRotation.y), DirectX::XMConvertToDegrees(mRotation.z));
}

const DirectX::XMFLOAT3 Transform::GetRotationRadians() const
{
	return DirectX::XMFLOAT3(mRotation.x, mRotation.y, mRotation.z);
}

const DirectX::XMFLOAT4 Transform::GetQuaternion() const
{
	DirectX::XMFLOAT4 result {}; 
	DirectX::XMVECTOR quaternion = DirectX::XMQuaternionRotationRollPitchYaw(mRotation.x, mRotation.y, mRotation.z);
	DirectX::XMStoreFloat4(&result, quaternion);
	return result;
}

void Transform::SetScale(float x, float y, float z)
{
	mScale.x = x;
	mScale.y = y;
	mScale.z = z;
}

const DirectX::XMFLOAT3& Transform::GetScale() const
{
	return mScale;
}

void Transform::AddTranslation(float x, float y, float z)
{
	mPosition.x += x;
	mPosition.y += y;
	mPosition.z += z;
}

DirectX::XMFLOAT3 Transform::GetForward() const
{
	DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(mRotation.x, mRotation.y, mRotation.z);
	DirectX::XMFLOAT3 result = DirectX::XMFLOAT3(rotation.r[2].m128_f32[0], rotation.r[2].m128_f32[1], rotation.r[2].m128_f32[2]);
	return result;
}

DirectX::XMFLOAT3 Transform::GetRight() const
{
	DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(mRotation.x, mRotation.y, mRotation.z);
	DirectX::XMFLOAT3 result = DirectX::XMFLOAT3(rotation.r[0].m128_f32[0], rotation.r[0].m128_f32[1], rotation.r[0].m128_f32[2]);
	return result;
}

DirectX::XMFLOAT3 Transform::GetUp() const
{
	DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(mRotation.x, mRotation.y, mRotation.z);
	DirectX::XMFLOAT3 result = DirectX::XMFLOAT3(rotation.r[1].m128_f32[0], rotation.r[1].m128_f32[1], rotation.r[1].m128_f32[2]);
	return result;
}

DirectX::XMMATRIX& Transform::GetTransformMatrix() const
{
	DirectX::XMMATRIX s = DirectX::XMMatrixScaling(mScale.x, mScale.y, mScale.z);
	DirectX::XMMATRIX r = DirectX::XMMatrixRotationRollPitchYaw(mRotation.x, mRotation.y, mRotation.z);
	DirectX::XMMATRIX t = DirectX::XMMatrixTranslation(mPosition.x, mPosition.y, mPosition.z);
	DirectX::XMMATRIX result = s * r * t;

	return result;
}

DirectX::XMMATRIX& Transform::GetViewMatrix() const
{
	DirectX::XMFLOAT3 position = GetPosition();
	DirectX::XMFLOAT3 forward = GetForward();
	DirectX::XMFLOAT3 up = GetUp();

	DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat3(&position);
	DirectX::XMVECTOR forwardVector = DirectX::XMLoadFloat3(&forward);
	DirectX::XMVECTOR upVector = DirectX::XMLoadFloat3(&up);

	DirectX::XMMATRIX result = DirectX::XMMatrixLookToLH(positionVector, forwardVector, upVector);
	return result;
}

DirectX::XMMATRIX& Transform::GetPlanarReflectionsViewMatrix(const DirectX::XMFLOAT3& planeNormal, const DirectX::XMFLOAT3& planePosition) const
{
	DirectX::XMVECTOR planeNormalVector = DirectX::XMLoadFloat3(&planeNormal);
	DirectX::XMVECTOR planePositionVector = DirectX::XMLoadFloat3(&planePosition);

	DirectX::XMMATRIX viewMatrix = GetViewMatrix();

	float d = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(planeNormalVector, planePositionVector));
	DirectX::XMFLOAT4 plane(DirectX::XMVectorGetX(planeNormalVector), DirectX::XMVectorGetY(planeNormalVector), DirectX::XMVectorGetZ(planeNormalVector), d);

	DirectX::XMMATRIX reflectionMatrix = DirectX::XMMatrixReflect(XMLoadFloat4(&plane));

	DirectX::XMMATRIX reflectedViewMatrix = DirectX::XMMatrixMultiply(viewMatrix, reflectionMatrix);
	return reflectedViewMatrix;
}

DirectX::XMMATRIX& Transform::GetPlanarReflectionsTransform(const DirectX::XMFLOAT3& planeNormal, const DirectX::XMFLOAT3& planePosition) const
{
	DirectX::XMVECTOR planeNormalVector = DirectX::XMLoadFloat3(&planeNormal);
	DirectX::XMVECTOR planePositionVector = DirectX::XMLoadFloat3(&planePosition);

	float d = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(planeNormalVector, planePositionVector));
	DirectX::XMFLOAT4 plane(DirectX::XMVectorGetX(planeNormalVector), DirectX::XMVectorGetY(planeNormalVector), DirectX::XMVectorGetZ(planeNormalVector), d);

	DirectX::XMMATRIX reflectionMatrix = DirectX::XMMatrixReflect(XMLoadFloat4(&plane));

	DirectX::XMMATRIX s = DirectX::XMMatrixScaling(mScale.x, mScale.y, mScale.z);
	DirectX::XMMATRIX r = DirectX::XMMatrixRotationRollPitchYaw(mRotation.x, mRotation.y, mRotation.z);
	DirectX::XMMATRIX t = DirectX::XMMatrixTranslation(mPosition.x, mPosition.y, mPosition.z);
	DirectX::XMMATRIX result = s * r * t;

	result = reflectionMatrix * result;

	return result;
}

