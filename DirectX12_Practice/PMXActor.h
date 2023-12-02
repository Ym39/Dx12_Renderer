#pragma once
#include<d3d12.h>
#include<DirectXMath.h>
#include<vector>
#include<array>
#include<string>
#include<wrl.h>
#include<algorithm>
#include "PmxFileData.h"
#include "VMDFileData.h"

using namespace DirectX;

class Dx12Wrapper;
class PMXActor
{
public:
	PMXActor();
	~PMXActor();

	bool Initialize(const std::wstring& filePath, Dx12Wrapper& dx);
	void Update();
	void Draw(Dx12Wrapper& dx, bool isShadow);

private:
	HRESULT CreateVbAndIb(Dx12Wrapper& dx);
	HRESULT CreateTransformView(Dx12Wrapper& dx);
	HRESULT CreateMaterialData(Dx12Wrapper& dx);
	HRESULT CreateMaterialAndTextureView(Dx12Wrapper& dx);

private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	PMXFileData _pmxFileData;
	VMDFileData _vmdFileData;

	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;
	std::vector<ComPtr<ID3D12Resource>> _sphereTextureResources;

	ComPtr<ID3D12Resource> _transformMat = nullptr;
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;
	std::vector<XMMATRIX> _boneMatrices;

	struct Transform
	{
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	Transform _transform;
	DirectX::XMMATRIX* _mappedMatrices;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	ComPtr<ID3D12DescriptorHeap> _materialHeap = nullptr;

	struct MaterialForShader
	{
		XMFLOAT4 diffuse;
		XMFLOAT3 specular;
		float specularPower;
		XMFLOAT3 ambient;
	};
};

