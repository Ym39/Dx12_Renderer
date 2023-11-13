#include "PMXActor.h"
#include <array>
#include <bitset>
#include "Dx12Wrapper.h"
#include "PMXRenderer.h"
#include "srtconv.h"
#include <d3dx12.h>

using namespace std;

PMXActor::PMXActor()
{
}

PMXActor::~PMXActor()
{
}

void* PMXActor::Transform::operator new(size_t size) {
	return _aligned_malloc(size, 16);
}

bool PMXActor::Initialize(const std::wstring& filePath, Dx12Wrapper& dx)
{
	bool result = LoadPMXFile(filePath, _pmxFileData);
	if (result == false)
	{
		return false;
	}

	_transform.world = XMMatrixIdentity() * XMMatrixTranslation(0.0f, 0.0f, 0.0f);

	auto hResult = CreateVbAndIb(dx);
	if (FAILED(hResult))
	{
		return false;
	}

	hResult = CreateTransformView(dx);
	if (FAILED(hResult))
	{
		return false;
	}

	hResult = CreateMaterialData(dx);
	if (FAILED(hResult))
	{
		return false;
	}

	hResult = CreateMaterialAndTextureView(dx);
	if (FAILED(hResult))
	{
		return false;
	}

	return true;
}

void PMXActor::Update()
{
}

void PMXActor::Draw(Dx12Wrapper& dx, bool isShadow = false)
{
	dx.CommandList()->IASetVertexBuffers(0, 1, &_vbView);
	dx.CommandList()->IASetIndexBuffer(&_ibView);
	dx.CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* transheap[] = { _transformHeap.Get() };
	dx.CommandList()->SetDescriptorHeaps(1, transheap);
	dx.CommandList()->SetGraphicsRootDescriptorTable(1, _transformHeap->GetGPUDescriptorHandleForHeapStart());

	ID3D12DescriptorHeap* mdh[] = { _materialHeap.Get() };

	dx.CommandList()->SetDescriptorHeaps(1, mdh);

	auto materialH = _materialHeap->GetGPUDescriptorHandleForHeapStart();
	unsigned int idxOffset = 0;

	auto cbvSrvIncSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 4;

	if (isShadow == true)
	{
		dx.CommandList()->DrawIndexedInstanced(_pmxFileData.faces.size() * 3, 1, 0, 0, 0);
	}
	else
	{
		for (auto& material : _pmxFileData.materials)
		{
			dx.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
			dx.CommandList()->DrawIndexedInstanced(material.numFaceVertices, 1, idxOffset, 0, 0);
			materialH.ptr += cbvSrvIncSize;
			idxOffset += material.numFaceVertices;
		}
	}
}

HRESULT PMXActor::CreateVbAndIb(Dx12Wrapper& dx)
{
	D3D12_HEAP_PROPERTIES heapprop = {};

	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};

	size_t pmxVertexSize = sizeof(PMXVertex);

	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = _pmxFileData.vertices.size() * pmxVertexSize;
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	auto result = dx.Device()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(_vb.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	PMXVertex* vertexMap = nullptr;

	result = _vb->Map(0, nullptr, (void**)&vertexMap);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	std::copy(std::begin(_pmxFileData.vertices), std::end(_pmxFileData.vertices), vertexMap);
	_vb->Unmap(0, nullptr);

	_vbView.BufferLocation = _vb->GetGPUVirtualAddress();
	_vbView.SizeInBytes = pmxVertexSize * _pmxFileData.vertices.size();
	_vbView.StrideInBytes = pmxVertexSize;

	size_t faceSize = sizeof(PMXFace);
	resdesc.Width = _pmxFileData.faces.size() * faceSize;

	result = dx.Device()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(_ib.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	PMXFace* mappedFace = nullptr;
	_ib->Map(0, nullptr, (void**)&mappedFace);
	std::copy(std::begin(_pmxFileData.faces), std::end(_pmxFileData.faces), mappedFace);
	_ib->Unmap(0, nullptr);

	_ibView.BufferLocation = _ib->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R32_UINT;
	_ibView.SizeInBytes = _pmxFileData.faces.size() * faceSize;

	const size_t materialNum = _pmxFileData.materials.size();
	_textureResources.resize(materialNum);
	_toonResources.resize(materialNum);
	_sphereTextureResources.resize(materialNum);

	wstring folderPath = L"PMXModel/";

	for (int i = 0; i < materialNum; ++i)
	{
		PMXMaterial& curMaterial = _pmxFileData.materials[i];

		_textureResources[i] = nullptr;
		_toonResources[i] = nullptr;
		_sphereTextureResources[i] = nullptr;

		int textureIndex = curMaterial.textureIndex;

		if (_pmxFileData.textures.size() - 1 >= textureIndex)
		{
			wstring textureFileName = _pmxFileData.textures[textureIndex].textureName;
			if (textureFileName.empty() == false)
			{
				_textureResources[i] = dx.GetTextureByPath(folderPath + textureFileName);
			}
		}

		int toonIndex = curMaterial.toonTextureIndex;

		if (toonIndex != 0 && _pmxFileData.textures.size() - 1 >= toonIndex)
		{
			wstring toonTextureFileName = _pmxFileData.textures[toonIndex].textureName;
			if (toonTextureFileName.empty() == false)
			{
				_toonResources[i] = dx.GetTextureByPath(folderPath + toonTextureFileName);
			}
		}

		int sphereTextureIndex = curMaterial.sphereTextureIndex;

		if (sphereTextureIndex != 0 && _pmxFileData.textures.size() - 1 >= sphereTextureIndex)
		{
			wstring sphereTextureFileName = _pmxFileData.textures[sphereTextureIndex].textureName;
			if (sphereTextureFileName.empty() == false)
			{
				_sphereTextureResources[i] = dx.GetTextureByPath(folderPath + sphereTextureFileName);
			}
		}
	}

	return S_OK;
}

HRESULT PMXActor::CreateTransformView(Dx12Wrapper& dx)
{
	_boneMatrices.resize(_pmxFileData.bones.size());
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	auto buffSize = sizeof(Transform) * (1 + _boneMatrices.size());
	buffSize = (buffSize + 0xff) & ~0xff;

	auto result = dx.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_transformBuff.ReleaseAndGetAddressOf())
	);

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	result = _transformBuff->Map(0, nullptr, (void**)&_mappedMatrices);

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	_mappedMatrices[0] = _transform.world;

	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};

	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;
	transformDescHeapDesc.NumDescriptors = 1;
	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = dx.Device()->CreateDescriptorHeap(&transformDescHeapDesc, IID_PPV_ARGS(_transformHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _transformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;

	dx.Device()->CreateConstantBufferView(&cbvDesc, _transformHeap->GetCPUDescriptorHandleForHeapStart());

	std::copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);

	return S_OK;
}

HRESULT PMXActor::CreateMaterialData(Dx12Wrapper& dx)
{
	int materialBufferSize = sizeof(MaterialForShader);
	materialBufferSize = (materialBufferSize + 0xff) & ~0xff;

	auto result = dx.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize * _pmxFileData.materials.size()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_materialBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	char* mapMaterial = nullptr;

	result = _materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	for (const auto& material : _pmxFileData.materials)
	{
		MaterialForShader* uploadMat = reinterpret_cast<MaterialForShader*>(mapMaterial);
		uploadMat->diffuse = material.diffuse;
		uploadMat->specular = material.specular;
		uploadMat->specularPower = material.specularPower;
		uploadMat->ambient = material.ambient;

		mapMaterial += materialBufferSize;
	}

	_materialBuff->Unmap(0, nullptr);

	return S_OK;
}

HRESULT PMXActor::CreateMaterialAndTextureView(Dx12Wrapper& dx)
{
	D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
	matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	matDescHeapDesc.NodeMask = 0;
	matDescHeapDesc.NumDescriptors = _pmxFileData.materials.size() * 4;
	matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	auto result = dx.Device()->CreateDescriptorHeap(&matDescHeapDesc, IID_PPV_ARGS(_materialHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	auto materialBuffSize = sizeof(MaterialForShader);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = _materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = materialBuffSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	auto matDescHeapH = _materialHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int i = 0; i < _pmxFileData.materials.size(); ++i)
	{
		dx.Device()->CreateConstantBufferView(&matCBVDesc, matDescHeapH);

		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;

		if (_textureResources[i] == nullptr)
		{
			ComPtr<ID3D12Resource> whiteTexture = dx.GetWhiteTexture();
			srvDesc.Format = whiteTexture->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(whiteTexture.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _textureResources[i]->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(_textureResources[i].Get(),&srvDesc,matDescHeapH);
		}

		matDescHeapH.ptr += incSize;

		if (_toonResources[i] == nullptr)
		{
			ComPtr<ID3D12Resource> gradTexture = dx.GetWhiteTexture();
			srvDesc.Format = gradTexture->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(gradTexture.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _toonResources[i]->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(_toonResources[i].Get(), &srvDesc, matDescHeapH);
		}

		matDescHeapH.ptr += incSize;

		if (_sphereTextureResources[i] == nullptr)
		{
			ComPtr<ID3D12Resource> whiteTexture = dx.GetWhiteTexture();
			srvDesc.Format = whiteTexture->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(whiteTexture.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _sphereTextureResources[i]->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(_sphereTextureResources[i].Get(), &srvDesc, matDescHeapH);
		}

		matDescHeapH.ptr += incSize;
	}

	return result;
}

