#include "FBXActor.h"

#include <d3dx12.h>

#include "Dx12Wrapper.h"
#include "Transform.h"
#include "BoundBox.h"
#include "MaterialManager.h"
#include "ImguiManager.h"
#include "Imgui/imgui.h"

FBXActor::FBXActor() 
{
}

bool FBXActor::Initialize(const std::string& path, Dx12Wrapper& dx)
{
	mModelPath = path;

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
	mMeshMaterialNameList.resize(mMeshes.size());

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

	std::vector<DirectX::XMFLOAT3> vertexPositionList;
	vertexPositionList.reserve(mVertices.size());

	for (const auto& vertex : mVertices)
	{
		vertexPositionList.push_back(vertex.position);
	}

	mBounds = BoundsBox::CreateBoundsBox(vertexPositionList);

	return true;
}

void FBXActor::SetMaterialName(const std::vector<std::string> materialNameList)
{
	for (int i = 0; i < mMeshMaterialNameList.size(); i++)
	{
		if (materialNameList.size() <= i)
		{
			continue;
		}

		mMeshMaterialNameList[i] = materialNameList[i];
	}
}

void FBXActor::Draw(Dx12Wrapper& dx, bool isShadow) const
{
	dx.CommandList()->IASetVertexBuffers(0, 1, &mVertexBufferView);
	dx.CommandList()->IASetIndexBuffer(&mIndexBufferView);
	dx.CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* transformHeap[] = { mTransformHeap.Get() };
	dx.CommandList()->SetDescriptorHeaps(1, transformHeap);
	dx.CommandList()->SetGraphicsRootDescriptorTable(1, mTransformHeap->GetGPUDescriptorHandleForHeapStart());

	MaterialManager::Instance().SetMaterialDescriptorHeaps(dx);

	unsigned int indexOffset = 0;

	for (int i = 0; i < mMeshes.size(); i++)
	{
		MaterialManager::Instance().SetGraphicsRootDescriptorTableMaterial(dx, 2, mMeshMaterialNameList[i]);

		unsigned int indexCount = mMeshes[i].indexCount;

		dx.CommandList()->DrawIndexedInstanced(indexCount, 1, indexOffset, 0, 0);

		indexOffset += indexCount;
	}

	//dx.CommandList()->DrawIndexedInstanced(mIndexCount, 1, indexOffset, 0, 0);
}

void FBXActor::Update()
{
	mMappedWorldTransform[0] = mTransform.GetTransformMatrix();
}

Transform& FBXActor::GetTransform()
{
	return mTransform;
}

TypeIdentity FBXActor::GetType()
{
	return TypeIdentity::FbxActor;
}

bool FBXActor::TestSelect(int mouseX, int mouseY, DirectX::XMFLOAT3 cameraPosition, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix)
{
	if (mBounds == nullptr)
	{
		return false;
	}

	DirectX::XMFLOAT3 worldPosition = mTransform.GetPosition();

	return mBounds->TestIntersectionBoundsBoxByMousePosition(mouseX, mouseY, worldPosition, cameraPosition, viewMatrix, projectionMatrix);
}

std::string FBXActor::GetName() const
{
	return mName;
}

void FBXActor::SetName(std::string name)
{
	mName = name;
}

void FBXActor::UpdateImGui(Dx12Wrapper& dx)
{
	ImguiManager::Instance().DrawTransformUI(mTransform);
	std::vector<std::string> selectNameList(mMeshMaterialNameList);
	const auto& materialNameList = MaterialManager::Instance().GetNameList();
	const char** nameItems = new const char* [materialNameList.size()];

	for (int i = 0; i < materialNameList.size(); i++)
	{
		nameItems[i] = materialNameList[i].c_str();
	}

	ImGui::Text("Material");
	for (int i = 0; i < mMeshMaterialNameList.size(); i++)
	{
		std::string comboName = "## fbxActorMaterialSelect" + std::to_string(i);

		int selectIndex = -1;

		for (int j = 0; j < materialNameList.size(); j++)
		{
			if (mMeshMaterialNameList[i] == materialNameList[j])
			{
				selectIndex = j;
				break;
			}
		}

		if (ImGui::Combo(comboName.c_str(), &selectIndex, nameItems, materialNameList.size()))
		{
			selectNameList[i] = materialNameList[selectIndex];
			SetMaterialName(selectNameList);
		}
	}
}

const std::vector<std::string> FBXActor::GetMaterialNameList() const
{
	return mMeshMaterialNameList;
}

void FBXActor::GetSerialize(json& j)
{
	json positionJson;
	Serialize::Float3ToJson(positionJson, mTransform.GetPosition());
	json rotationJson;
	Serialize::Float3ToJson(rotationJson, mTransform.GetRotation());
	json scaleJson;
	Serialize::Float3ToJson(scaleJson, mTransform.GetScale());

	j["ModelPath"] = mModelPath;

	json transformJson;
	transformJson["Position"] = positionJson;
	transformJson["Rotation"] = rotationJson;
	transformJson["Scale"] = scaleJson;

	j["Transform"] = transformJson;

	json materialNameJson;
	for (int i = 0; i < mMeshMaterialNameList.size(); i++)
	{
		materialNameJson[i] = mMeshMaterialNameList[i];
	}

	j["Material"] = materialNameJson;
	j["Name"] = mName;
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

		mVertices.push_back(vertex);
		vertexCount++;
	}

	unsigned int indexCount = 0;
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
		{
			mIndices.push_back(face.mIndices[j] + mVertexCount);
			indexCount++;
		}
	}

	auto meshInfo = FBXMesh();
	meshInfo.startVertex = mVertexCount;
	meshInfo.vertexCount = vertexCount;
	meshInfo.startIndex = mIndexCount;
	meshInfo.indexCount = indexCount;
	mMeshes.push_back(meshInfo);

	mVertexCount += vertexCount;
	mIndexCount += indexCount;
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
	resourceDesc.Width = mVertexCount * vertexSize;
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
		IID_PPV_ARGS(mVertexBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = mVertexBuffer->Map(0, nullptr, (void**)&mMappedVertex);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	std::copy(std::begin(mVertices), std::end(mVertices), mMappedVertex);
	mVertexBuffer->Unmap(0, nullptr);

	mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
	mVertexBufferView.SizeInBytes = vertexSize * mVertexCount;
	mVertexBufferView.StrideInBytes = vertexSize;

	resourceDesc.Width = sizeof(unsigned int) * mIndexCount;

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
	mIndexBufferView.SizeInBytes = sizeof(unsigned int) * mIndexCount;

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
		IID_PPV_ARGS(mTransformBuffer.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = mTransformBuffer->Map(0, nullptr, (void**)&mMappedWorldTransform);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	mMappedWorldTransform[0] = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(100.0f, 100.0f, 100.0f);

	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;
	transformDescHeapDesc.NumDescriptors = 1;
	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = dx.Device()->CreateDescriptorHeap(
		&transformDescHeapDesc,
		IID_PPV_ARGS(mTransformHeap.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = mTransformBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;

	dx.Device()->CreateConstantBufferView(&cbvDesc, mTransformHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}
 