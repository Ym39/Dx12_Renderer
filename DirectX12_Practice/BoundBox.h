#pragma once
#include <DirectXMath.h>
#include <vector>

class BoundsBox
{
public:
	static BoundsBox* CreateBoundsBox(const std::vector<DirectX::XMFLOAT3>& vertices);

	bool TestIntersectionBoundsBoxByMousePosition(
		int mouseX, 
		int mouseY, 
		DirectX::XMFLOAT3 worldPosition,
		DirectX::XMFLOAT3 cameraPosition, 
		const DirectX::XMMATRIX& viewMatrix, 
		const DirectX::XMMATRIX& projectionMatrix) const;

private:
	DirectX::XMFLOAT3 mCenter;
	DirectX::XMFLOAT3 mExents;
	DirectX::XMFLOAT3 mMin;
	DirectX::XMFLOAT3 mMax;
};

