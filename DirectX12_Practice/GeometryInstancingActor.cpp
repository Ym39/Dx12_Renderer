#include "GeometryInstancingActor.h"

#include <algorithm>
#include <d3dx12.h>
#include <random>

#include "Dx12Wrapper.h"
#include "Transform.h"
#include "Imgui/imgui.h"

GeometryInstancingActor::GeometryInstancingActor(const CubeGeometry& cube, unsigned int instancingCount = 1):
mVertices(cube.GetVertices()),
mIndices(cube.GetIndices()),
mInstanceCount(instancingCount),
mInstanceUnit(10.0f),
mTransform(new Transform())
{
}

GeometryInstancingActor::~GeometryInstancingActor()
{
}

void GeometryInstancingActor::Initialize(Dx12Wrapper& dx)
{
	HRESULT result;
	InitializeInstanceData(mInstanceCount);

	result = CreateVertexBufferAndIndexBuffer(dx);
	assert(SUCCEEDED(result));

	result = CreateInstanceBuffer(dx);
	assert(SUCCEEDED(result));

	result = UploadInstanceData(dx);
	assert(SUCCEEDED(result));
}

void GeometryInstancingActor::Draw(Dx12Wrapper& dx, bool isShadow) const
{
	dx.SetSceneBuffer(0);
	dx.SetGlobalParameterBuffer(2);
	dx.SetRSSetViewportsAndScissorRectsByScreenSize();

	auto cmdList = dx.CommandList();
	cmdList->SetGraphicsRootShaderResourceView(1, mInstanceBuffer->GetGPUVirtualAddress());
	
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetVertexBuffers(0, 1, &mVertexBufferView);
	cmdList->IASetIndexBuffer(&mIndexBufferView);
	cmdList->DrawIndexedInstanced(GetIndexCount(), mInstanceCount, 0, 0, 0);
}

void GeometryInstancingActor::Update()
{
}

void GeometryInstancingActor::EndOfFrame(Dx12Wrapper& dx)
{
	if (mRefreshInstanceBufferFlag == false)
	{
		return;
	}

	mRefreshInstanceBufferFlag = false;
	ChangeInstanceCount(dx);
}

Transform& GeometryInstancingActor::GetTransform()
{
	return *mTransform;
}

int GeometryInstancingActor::GetIndexCount() const
{
	return mIndices.size();
}

std::string GeometryInstancingActor::GetName() const
{
	return mName;
}

void GeometryInstancingActor::SetName(std::string name)
{
	mName = name;
}

void GeometryInstancingActor::UpdateImGui(Dx12Wrapper& dx)
{
	const DirectX::XMFLOAT3& position = mTransform->GetPosition();
	const DirectX::XMFLOAT3& rotation = mTransform->GetRotation();
	const DirectX::XMFLOAT3& scale = mTransform->GetScale();

	static int instanceCount = mInstanceCount;

	float positionArray[] = { position.x, position.y, position.z };
	float rotationArray[] = { rotation.x, rotation.y, rotation.z };
	float scaleArray[] = { scale.x, scale.y, scale.z };

	if (ImGui::DragFloat3("Position ## Actor", positionArray, 0.01f))
	{
		mTransform->SetPosition(positionArray[0], positionArray[1], positionArray[2]);
	}

	if (ImGui::DragFloat3("Rotation ## Actor", rotationArray, 0.01f))
	{
		mTransform->SetRotation(rotationArray[0], rotationArray[1], rotationArray[2]);
	}

	if (ImGui::DragFloat3("Scale ## Actor", scaleArray, 0.01f))
	{
		mTransform->SetScale(scaleArray[0], scaleArray[1], scaleArray[2]);
	}

	ImGui::InputInt("Instance Count ## Actor Inspector", &instanceCount);

	if (ImGui::Button("Apply") == true)
	{
		mInstanceCount = instanceCount;
		mRefreshInstanceBufferFlag = true;
	}
}

void GeometryInstancingActor::InitializeInstanceData(unsigned int instanceCount)
{
	mInstanceData.clear();
	mInstanceData.reserve(instanceCount);

	const unsigned int rowCount = 50;

	DirectX::XMVECTOR rootPosition = DirectX::XMLoadFloat3(&mTransform->GetPosition());
	DirectX::XMMATRIX rootWorld = mTransform->GetTransformMatrix();

	int r = 0;
	int c = 0;

    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<int> distribution(0, 2);

	DirectX::XMFLOAT4 colors[] =
	{
		DirectX::XMFLOAT4(0.259f, 0.918f, 0.996f, 1.0f),
		DirectX::XMFLOAT4(0.565f, 0.792f, 0.624f, 1.0f),
		DirectX::XMFLOAT4(1.0f, 0.471f, 0.561f, 1.0f)
	};

	for (int i = 0; i < instanceCount; i++)
	{
		DirectX::XMVECTOR positionVector = DirectX::XMVectorAdd(rootPosition, DirectX::XMVectorSet(mInstanceUnit * r, 0.0f, mInstanceUnit * c, 0.0f));
		DirectX::XMFLOAT4 color = colors[distribution(generator)];
		DirectX::XMFLOAT3 position{};
		DirectX::XMStoreFloat3(&position, positionVector);

		DirectX::XMMATRIX world = rootWorld * DirectX::XMMatrixTranslation(mInstanceUnit * r, 0.0f, mInstanceUnit * c);

		mInstanceData.emplace_back(position, mTransform->GetQuaternion(), mTransform->GetScale(), color);
		
		//mInstanceData.emplace_back(world, color);

		if (i != 0 && i % rowCount == 0)
		{
			c++;
			r = 0;
		}
		else
		{
			r++;
		}
	}
}

HRESULT GeometryInstancingActor::CreateVertexBufferAndIndexBuffer(Dx12Wrapper& dx)
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

HRESULT GeometryInstancingActor::CreateInstanceBuffer(Dx12Wrapper& dx)
{
	auto bufferSize = sizeof(GeometryInstanceData);

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize * mInstanceCount);

	auto result = dx.Device()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&mInstanceBuffer)
	);

	if (FAILED(result))
	{
		return result;
	}

	heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	result = dx.Device()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mInstanceUploadBuffer)
	);

	return result;
}

HRESULT GeometryInstancingActor::UploadInstanceData(Dx12Wrapper& dx)
{
	GeometryInstanceData* mappedData;

	const size_t instanceDataSize = sizeof(GeometryInstanceData);

	auto result = mInstanceUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
	if (FAILED(result))
	{
		return result;
	}

	memcpy(mappedData, mInstanceData.data(), instanceDataSize * mInstanceCount);

	mInstanceUploadBuffer->Unmap(0, nullptr);

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = mappedData;
	subResourceData.RowPitch = instanceDataSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	UpdateSubresources(dx.CommandList().Get(), mInstanceBuffer.Get(), mInstanceUploadBuffer.Get(), 0, 0, 1, &subResourceData);

	return S_OK;
}

void GeometryInstancingActor::ChangeInstanceCount(Dx12Wrapper& dx)
{
	InitializeInstanceData(mInstanceCount);

	mInstanceBuffer = nullptr;
	mInstanceUploadBuffer = nullptr;

	auto result = CreateInstanceBuffer(dx);
	assert(SUCCEEDED(result));

	result = UploadInstanceData(dx);
	assert(SUCCEEDED(result));
}
