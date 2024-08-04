#include "GeometryActor.h"

#include <d3dx12.h>

#include "Dx12Wrapper.h"
#include "Transform.h"
#include "ImguiManager.h"

GeometryActor::GeometryActor(const IGeometry& geometry):
mVertices(geometry.GetVertices()),
mIndices(geometry.GetIndices()),
mTransform(new Transform())
{
}

void GeometryActor::Initialize(Dx12Wrapper& dx)
{
	HRESULT result = CreateVertexBufferAndIndexBuffer(dx);
	assert(SUCCEEDED(result));

	result = CreateTransformBuffer(dx);
	assert(SUCCEEDED(result));
}

void GeometryActor::Draw(Dx12Wrapper& dx) const
{
	dx.SetSceneBuffer(0);
	dx.SetRSSetViewportsAndScissorRectsByScreenSize();

	auto cmdList = dx.CommandList();
	cmdList->SetGraphicsRootConstantBufferView(1, mTransformBuffer->GetGPUVirtualAddress());

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetVertexBuffers(0, 1, &mVertexBufferView);
	cmdList->IASetIndexBuffer(&mIndexBufferView);
	cmdList->DrawIndexedInstanced(mIndices.size(), 1, 0, 0, 0);
}

void GeometryActor::Update()
{
	DirectX::XMMATRIX* mappedTransform;
	mTransformBuffer->Map(0, nullptr, (void**)&mappedTransform);
	mappedTransform[0] = mTransform->GetTransformMatrix();
	mTransformBuffer->Unmap(0, nullptr);
}

void GeometryActor::EndOfFrame(Dx12Wrapper& dx)
{
}

Transform& GeometryActor::GetTransform()
{
	return *mTransform;
}

std::string GeometryActor::GetName() const
{
	return mName;
}

void GeometryActor::SetName(std::string name)
{
	mName = name;
}

void GeometryActor::UpdateImGui(Dx12Wrapper& dx)
{
	ImguiManager::Instance().DrawTransformUI(*mTransform);
}

HRESULT GeometryActor::CreateVertexBufferAndIndexBuffer(Dx12Wrapper& dx)
{
	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	size_t vertexSize = sizeof(Vertex);
	size_t vertexCount = mVertices.size();
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = vertexCount * vertexSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	auto result = dx.Device()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mVertexBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = mVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedVertex));
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	std::copy(std::begin(mVertices), std::end(mVertices), mMappedVertex);
	mVertexBuffer->Unmap(0, nullptr);

	mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
	mVertexBufferView.SizeInBytes = vertexSize * vertexCount;
	mVertexBufferView.StrideInBytes = vertexSize;

	resourceDesc.Width = sizeof(unsigned int) * mIndices.size();

	result = dx.Device()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mIndexBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = mIndexBuffer->Map(0, nullptr, (void**)&mMappedIndex);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	std::copy(std::begin(mIndices), std::end(mIndices), mMappedIndex);
	mIndexBuffer->Unmap(0, nullptr);

	mIndexBufferView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
	mIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	mIndexBufferView.SizeInBytes = sizeof(unsigned int) * mIndices.size();

	return S_OK;
}

HRESULT GeometryActor::CreateTransformBuffer(Dx12Wrapper& dx)
{
	auto buffSize = sizeof(DirectX::XMMATRIX);
	buffSize = (buffSize + 0xff) & ~0xff;

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(buffSize);

	auto result = dx.Device()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mTransformBuffer.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	DirectX::XMMATRIX* mappedTransform;
	result = mTransformBuffer->Map(0, nullptr, (void**)&mappedTransform);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	mappedTransform[0] = mTransform->GetTransformMatrix();

	mTransformBuffer->Unmap(0, nullptr);

	return S_OK;
}
