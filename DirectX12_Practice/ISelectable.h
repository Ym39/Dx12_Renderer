#pragma once
#include <DirectXMath.h>

class ISelectable
{
public:
	virtual bool TestSelect(int mouseX, int mouseY, DirectX::XMFLOAT3 cameraPosition, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix) = 0;
};
