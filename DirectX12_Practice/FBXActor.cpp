#include "FBXActor.h"

#include <d3dx12.h>

#include "Dx12Wrapper.h"

bool FBXActor::Initialize(const std::string& path, Dx12Wrapper& dx)
{
	Assimp::Importer importer;

	unsigned int flag;
	flag = aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices|
		aiProcess_CalcTangentSpace |
		aiProcess_GenNormals |
		aiProcess_MakeLeftHanded |
		aiProcess_FlipWindingOrder |
		aiProcess_FlipUVs;

	const aiScene* scene = importer.ReadFile(path, flag);

	if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr)
	{
		return false;
	}

	ReadNode(scene->mRootNode, scene);

	auto hResult = CreateVertexBufferAndIndexBuffer(dx);
	if (FAILED(hResult))
	{
		return false;
	}

	hResult = CreateTransformView(dx);
	if (FAILED(hResult))
	{
		return false;
	}

	return true;
}

void FBXActor::Draw(Dx12Wrapper& dx, bool isShadow)
{
	dx.CommandList()->IASetVertexBuffers(0, 1, &_vertexBufferView);
	dx.CommandList()->IASetIndexBuffer(&_indexBufferView);
	dx.CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* transformHeap[] = { _transformHeap.Get() };
	dx.CommandList()->SetDescriptorHeaps(1, transformHeap);
	dx.CommandList()->SetGraphicsRootDescriptorTable(1, _transformHeap->GetGPUDescriptorHandleForHeapStart());

	unsigned int indexOffset = 0;

	for (int i = 0; i < _meshes.size(); i++)
	{
		unsigned int indexCount = _meshes[i].indexCount;

		dx.CommandList()->DrawIndexedInstanced(indexCount, 1, indexOffset, 0, 0);

		indexOffset += indexCount;
	}

	//dx.CommandList()->DrawIndexedInstanced(_indexCount, 1, indexOffset, 0, 0);
}

void FBXActor::Update()
{
}

DirectX::XMFLOAT3 FBXActor::AiVector3ToXMFLOAT3(const aiVector3D& vec)
{
	return DirectX::XMFLOAT3(vec.x, vec.y, vec.z);
}

DirectX::XMFLOAT2 FBXActor::AiVector2ToXMFLOAT2(const aiVector2D& vec)
{
	return DirectX::XMFLOAT2(vec.x, vec.y);
}

void FBXActor::ReadNode(aiNode* node, const aiScene* scene)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		ReadMesh(mesh, scene);
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) 
	{
		ReadNode(node->mChildren[i], scene);
	}
}

void FBXActor::ReadMesh(aiMesh* mesh, const aiScene* scene)
{
	unsigned int vertexCount = 0;
	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		FBXVertex vertex;
		vertex.position = AiVector3ToXMFLOAT3(mesh->mVertices[i]);
		vertex.normal = AiVector3ToXMFLOAT3(mesh->mNormals[i]);

		if (mesh->mTextureCoords[0] != nullptr)
		{
			vertex.uv.x = mesh->mTextureCoords[0][i].x;
			vertex.uv.y = mesh->mTextureCoords[0][i].y;
		}
		else
		{
			vertex.uv = DirectX::XMFLOAT2(0.f, 0.f);
		}

		_vertices.push_back(vertex);
		vertexCount++;
	}

	unsigned int indexCount = 0;
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
		{
			_indices.push_back(face.mIndices[j] + _vertexCount);
			indexCount++;
		}
	}

	auto meshInfo = FBXMesh();
	meshInfo.startVertex = _vertexCount;
	meshInfo.vertexCount = vertexCount;
	meshInfo.startIndex = _indexCount;
	meshInfo.indexCount = indexCount;
	_meshes.push_back(meshInfo);

	_vertexCount += vertexCount;
	_indexCount += indexCount;
}

HRESULT FBXActor::CreateVertexBufferAndIndexBuffer(Dx12Wrapper& dx)
{
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	size_t vertexSize = sizeof(FBXVertex);
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = _vertexCount * vertexSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	auto result = dx.Device()->CreateCommittedResource(&heapProperties, 
		D3D12_HEAP_FLAG_NONE, 
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_vertexBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = _vertexBuffer->Map(0, nullptr, (void**)&_mappedVertex);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	std::copy(std::begin(_vertices), std::end(_vertices), _mappedVertex);
	_vertexBuffer->Unmap(0, nullptr);

	_vertexBufferView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
	_vertexBufferView.SizeInBytes = vertexSize * _vertexCount;
	_vertexBufferView.StrideInBytes = vertexSize;

	resourceDesc.Width = sizeof(unsigned int) * _indexCount;

	result = dx.Device()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_indexBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = _indexBuffer->Map(0, nullptr, (void**)&_mappedIndex);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	std::copy(std::begin(_indices), std::end(_indices), _mappedIndex);
	_indexBuffer->Unmap(0, nullptr);

	_indexBufferView.BufferLocation = _indexBuffer->GetGPUVirtualAddress();
	_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	_indexBufferView.SizeInBytes = sizeof(unsigned int) * _indexCount;

	return S_OK;
}

HRESULT FBXActor::CreateTransformView(Dx12Wrapper& dx)
{
	auto buffSize = sizeof(DirectX::XMMATRIX);
	buffSize = (buffSize + 0xff) & ~0xff;

	auto result = dx.Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_transformBuffer.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = _transformBuffer->Map(0, nullptr, (void**)&_mappedWorldTranform);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	_mappedWorldTranform[0] = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(100.0f, 100.0f, 100.0f);

	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;
	transformDescHeapDesc.NumDescriptors = 1;
	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = dx.Device()->CreateDescriptorHeap(
		&transformDescHeapDesc,
		IID_PPV_ARGS(_transformHeap.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _transformBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;

	dx.Device()->CreateConstantBufferView(&cbvDesc, _transformHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}
 