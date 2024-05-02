#include "BoundBox.h"
#include "Define.h"
#include <Windows.h>
#include <minwindef.h>

BoundsBox* BoundsBox::CreateBoundsBox(const std::vector<DirectX::XMFLOAT3>& vertices)
{
	float minX = 0.0f, maxX = 0.0f;
	float minY = 0.0f, maxY = 0.0f;
	float minZ = 0.0f, maxZ = 0.0f;

	for (const auto& position : vertices)
	{
		minX = minX > position.x ? position.x : minX;
		maxX = maxX < position.x ? position.x : maxX;
		minY = minY > position.y ? position.y : minY;
		maxY = maxY < position.y ? position.y : maxY;
		minZ = minZ > position.z ? position.z : minZ;
		maxZ = maxZ < position.z ? position.z : maxZ;
	}

	BoundsBox* boundsBox = new BoundsBox;

	DirectX::XMFLOAT3 center = DirectX::XMFLOAT3((maxX + minX) * 0.5f, (maxY + minY) * 0.5f, (maxZ + minZ) * 0.5f);
	DirectX::XMFLOAT3 exents = DirectX::XMFLOAT3((maxX + minX) * 0.5f, (maxY + minY) * 0.5f, (maxZ + minZ) * 0.5f);

	boundsBox->mCenter = center;
	boundsBox->mExents = exents;
	boundsBox->mMin = DirectX::XMFLOAT3(center.x - exents.x, center.y - exents.y, center.z - exents.z);
	boundsBox->mMax = DirectX::XMFLOAT3(center.x + exents.x, center.y + exents.y, center.z + exents.z);

	return boundsBox;
}

bool BoundsBox::TestIntersectionBoundsBoxByMousePosition(int mouseX, int mouseY, DirectX::XMFLOAT3 worldPosition, DirectX::XMFLOAT3 cameraPosition, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix) const
{
	float pointX = 2.0f * static_cast<float>(mouseX) / static_cast<float>(window_width) - 1.0f;
	float pointY = (2.0f * static_cast<float>(mouseY) / static_cast<float>(window_height) - 1.0f) * -1.0f;

	DirectX::XMFLOAT3X3 projectionMatrix4{};
	XMStoreFloat3x3(&projectionMatrix4, projectionMatrix);

	pointX = pointX / projectionMatrix4._11;
	pointY = pointY / projectionMatrix4._22;

	DirectX::XMFLOAT3X3 inverseViewMatrix4{};
	XMStoreFloat3x3(&inverseViewMatrix4, XMMatrixInverse(nullptr, viewMatrix));

	DirectX::XMFLOAT3 direction{};
	direction.x = (pointX * inverseViewMatrix4._11) + (pointY * inverseViewMatrix4._21) + inverseViewMatrix4._31;
	direction.y = (pointX * inverseViewMatrix4._12) + (pointY * inverseViewMatrix4._22) + inverseViewMatrix4._32;
	direction.z = (pointX * inverseViewMatrix4._13) + (pointY * inverseViewMatrix4._23) + inverseViewMatrix4._33;

	DirectX::XMFLOAT3 rayOrigin{};
	DirectX::XMFLOAT3 rayDirection{};
	DirectX::XMStoreFloat3(&rayOrigin, DirectX::XMVectorSet(cameraPosition.x, cameraPosition.y, cameraPosition.z, 0.f));
	DirectX::XMStoreFloat3(&rayDirection, DirectX::XMVector3Normalize(DirectX::XMVectorSet(direction.x, direction.y, direction.z, 0.f)));

	DirectX::XMMATRIX worldTransformMatrix = DirectX::XMMatrixTranslation(worldPosition.x, worldPosition.y, worldPosition.z);

	DirectX::XMFLOAT3 worldMin{};
	DirectX::XMStoreFloat3(&worldMin, DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(mMin.x, mMin.y, mMin.z, 0.0f), worldTransformMatrix));

	DirectX::XMFLOAT3 worldMax{};
	DirectX::XMStoreFloat3(&worldMax, DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(mMax.x, mMax.y, mMax.z, 0.0f), worldTransformMatrix));

	DirectX::XMFLOAT3 dirfrac(1.f / rayDirection.x, 1.f / rayDirection.y, 1.f / rayDirection.z);

	float t1 = (worldMin.x - rayOrigin.x) * dirfrac.x;
	float t2 = (worldMax.x - rayOrigin.x) * dirfrac.x;
	float t3 = (worldMin.y - rayOrigin.y) * dirfrac.y;
	float t4 = (worldMax.y - rayOrigin.y) * dirfrac.y;
	float t5 = (worldMin.z - rayOrigin.z) * dirfrac.z;
	float t6 = (worldMax.z - rayOrigin.z) * dirfrac.z;

	float tmin = max(max(min(t1, t2), min(t3, t4)), min(t5, t6));
	float tmax = min(min(max(t1, t2), max(t3, t4)), max(t5, t6));

	float t;
	if (tmax < 0)
	{
		t = tmax;
		return false;
	}

	if (tmin > tmax)
	{
		t = tmax;
		return false;
	}

	t = tmin;
	return true;
}
