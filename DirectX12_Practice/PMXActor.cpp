#include "PMXActor.h"
#include "Dx12Wrapper.h"
#include "PMXRenderer.h"
#include "srtconv.h"
#include "MathUtil.h"

#include "RigidBody.h"
#include "Joint.h"

#include <array>
#include <bitset>
#include <algorithm>
#include <d3dx12.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>

#include "Time.h"
#include "UnicodeUtil.h"

using namespace std;

constexpr float epsilon = 0.0005f;

PMXActor::PMXActor()
{
}

PMXActor::~PMXActor()
{
	_nodeManager.Dispose();
}

bool PMXActor::Initialize(const std::wstring& filePath, Dx12Wrapper& dx)
{
	bool result = LoadPMXFile(filePath, _pmxFileData);
	if (result == false)
	{
		return false;
	}

	LoadVertexData(_pmxFileData.vertices);

	result = LoadVMDFile(L"VMD\\ラビットホール.vmd", _vmdFileData);
	if (result == false)
	{
		return false;
	}

	//_transform.world = XMMatrixIdentity() * XMMatrixTranslation(0.0f, 0.0f, 0.0f);

	InitParallelVertexSkinningSetting();

	_nodeManager.Init(_pmxFileData.bones);
	_morphManager.Init(_pmxFileData.morphs, _vmdFileData.morphs, _pmxFileData.vertices.size(), _pmxFileData.materials.size(), _pmxFileData.bones.size());

	InitAnimation(_vmdFileData);

	InitPhysics(_pmxFileData);

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

	ResetPhysics();

	return true;
}

void PMXActor::Update()
{
	_mappedMatrices[0] = _transformComp.GetTransformMatrix();
	_mappedReflectionMatrices[0] = _transformComp.GetPlanarReflectionsTransform(XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f));
}

void PMXActor::UpdateAnimation()
{
	if (_startTime <= 0)
	{
		_startTime = Time::GetTime();
	}

	unsigned int elapsedTime = Time::GetTime() - _startTime;
	unsigned int frameNo = 30 * (elapsedTime / 1000.0f);

	if (frameNo > _duration)
	{
		_startTime = Time::GetTime();
		frameNo = 0;
	}

	Time::RecordStartMorphUpdateTime();
	MorphMaterial();
	MorphBone();
	Time::EndMorphUpdate();

	Time::RecordStartAnimationUpdateTime();

	_nodeManager.BeforeUpdateAnimation();

	_morphManager.Animate(frameNo);
	_nodeManager.EvaluateAnimation(frameNo);

	_nodeManager.UpdateAnimation();

	UpdatePhysicsAnimation(elapsedTime);

	_nodeManager.UpdateAnimationAfterPhysics();

	Time::EndAnimationUpdate();

	Time::RecordStartSkinningUpdateTime();
	VertexSkinning();
	std::copy(_uploadVertices.begin(), _uploadVertices.end(), _mappedVertex);
	Time::EndSkinningUpdate();
}

void PMXActor::Draw(Dx12Wrapper& dx, bool isShadow = false) const
{
	dx.CommandList()->IASetVertexBuffers(0, 1, &_vbView);
	dx.CommandList()->IASetIndexBuffer(&_ibView);
	dx.CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* transheap[] = { _transformHeap.Get() };
	dx.CommandList()->SetDescriptorHeaps(1, transheap);
	dx.CommandList()->SetGraphicsRootDescriptorTable(1, _transformHeap->GetGPUDescriptorHandleForHeapStart());

	ID3D12DescriptorHeap* mdh[] = { _materialHeap.Get() };

	dx.CommandList()->SetDescriptorHeaps(1, mdh);

	if (isShadow == true)
	{
		dx.CommandList()->DrawIndexedInstanced(_pmxFileData.faces.size() * 3, 1, 0, 0, 0);
	}
	else
	{
		auto cbvSrvIncSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 4;

		auto materialH = _materialHeap->GetGPUDescriptorHandleForHeapStart();
		unsigned int idxOffset = 0;

		for (int i = 0; i < _pmxFileData.materials.size(); i++)
		{
			unsigned int numFaceVertices = _pmxFileData.materials[i].numFaceVertices;

			if (_loadedMaterial[i].visible == true)
			{
				dx.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
				dx.CommandList()->DrawIndexedInstanced(numFaceVertices, 1, idxOffset, 0, 0);
			}

			materialH.ptr += cbvSrvIncSize;
			idxOffset += numFaceVertices;
		}
	}
}

void PMXActor::DrawReflection(Dx12Wrapper& dx) const
{
	dx.CommandList()->IASetVertexBuffers(0, 1, &_vbView);
	dx.CommandList()->IASetIndexBuffer(&_ibView);
	dx.CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* transheap[] = { _transformHeap.Get() };
	dx.CommandList()->SetDescriptorHeaps(1, transheap);

	auto transformHandle = _transformHeap->GetGPUDescriptorHandleForHeapStart();
	auto incSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	transformHandle.ptr += incSize;
	dx.CommandList()->SetGraphicsRootDescriptorTable(1, transformHandle);

	ID3D12DescriptorHeap* mdh[] = { _materialHeap.Get() };

	dx.CommandList()->SetDescriptorHeaps(1, mdh);

	auto cbvSrvIncSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 4;

	auto materialH = _materialHeap->GetGPUDescriptorHandleForHeapStart();
	unsigned int idxOffset = 0;

	for (int i = 0; i < _pmxFileData.materials.size(); i++)
	{
		unsigned int numFaceVertices = _pmxFileData.materials[i].numFaceVertices;

		if (_loadedMaterial[i].visible == true)
		{
			dx.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
			dx.CommandList()->DrawIndexedInstanced(numFaceVertices, 1, idxOffset, 0, 0);
		}

		materialH.ptr += cbvSrvIncSize;
		idxOffset += numFaceVertices;
	}
}

const std::vector<LoadMaterial>& PMXActor::GetMaterials() const
{
	return _loadedMaterial;
}

void PMXActor::SetMaterials(const std::vector<LoadMaterial>& setMaterials)
{
	for (int i = 0; i < setMaterials.size(); i++)
	{
		if (_loadedMaterial.size() <= i)
		{
			break;
		}

		_loadedMaterial[i].visible = setMaterials[i].visible;
		_loadedMaterial[i].ambient = setMaterials[i].ambient;
		_loadedMaterial[i].diffuse = setMaterials[i].diffuse;
		_loadedMaterial[i].specular = setMaterials[i].specular;
		_loadedMaterial[i].specularPower = setMaterials[i].specularPower;
	}
}

Transform& PMXActor::GetTransform()
{
	return _transformComp;
}

std::string PMXActor::GetName() const
{
	return _name;
}

void PMXActor::SetName(std::string name)
{
	_name = name;
}

void PMXActor::UpdateImGui(Dx12Wrapper& dx)
{
}

HRESULT PMXActor::CreateVbAndIb(Dx12Wrapper& dx)
{
	D3D12_HEAP_PROPERTIES heapprop = {};

	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};

	size_t vertexSize = sizeof(UploadVertex);

	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = _uploadVertices.size() * vertexSize;
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

	result = _vb->Map(0, nullptr, (void**)&_mappedVertex);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	std::copy(std::begin(_uploadVertices), std::end(_uploadVertices), _mappedVertex);
	//_vb->Unmap(0, nullptr);

	_vbView.BufferLocation = _vb->GetGPUVirtualAddress();
	_vbView.SizeInBytes = vertexSize * _uploadVertices.size();
	_vbView.StrideInBytes = vertexSize;

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

	auto buffSize = sizeof(XMMATRIX);
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

	_mappedMatrices[0] = _transformComp.GetTransformMatrix();

	result = dx.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_reflectionTransformBuff.ReleaseAndGetAddressOf())
	);

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	result = _reflectionTransformBuff->Map(0, nullptr, (void**)&_mappedReflectionMatrices);

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	_mappedReflectionMatrices[0] = _transformComp.GetPlanarReflectionsTransform(XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f));

	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};

	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;
	transformDescHeapDesc.NumDescriptors = 2;
	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = dx.Device()->CreateDescriptorHeap(&transformDescHeapDesc, IID_PPV_ARGS(_transformHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _transformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;

	auto handle = _transformHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	dx.Device()->CreateConstantBufferView(&cbvDesc, handle);

	cbvDesc.BufferLocation = _reflectionTransformBuff->GetGPUVirtualAddress();
	handle.ptr+= incSize;

	dx.Device()->CreateConstantBufferView(&cbvDesc, handle);

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

	result = _materialBuff->Map(0, nullptr, (void**)&_mappedMaterial);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	_loadedMaterial.resize(_pmxFileData.materials.size());

	char* mappedMaterialPtr = _mappedMaterial;

	int materialIndex = 0;
	for (const auto& material : _pmxFileData.materials)
	{
		_loadedMaterial[materialIndex].visible = true;
		_loadedMaterial[materialIndex].name = UnicodeUtil::WstringToString(material.name);
		_loadedMaterial[materialIndex].diffuse = material.diffuse;
		_loadedMaterial[materialIndex].specular = material.specular;
		_loadedMaterial[materialIndex].specularPower = material.specularPower;
		_loadedMaterial[materialIndex].ambient = material.ambient;
		materialIndex++;

		MaterialForShader* uploadMat = reinterpret_cast<MaterialForShader*>(mappedMaterialPtr);
		uploadMat->diffuse = material.diffuse;
		uploadMat->specular = material.specular;
		uploadMat->specularPower = material.specularPower;
		uploadMat->ambient = material.ambient;

		mappedMaterialPtr += materialBufferSize;
	}

	//_materialBuff->Unmap(0, nullptr);

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

void PMXActor::LoadVertexData(const std::vector<PMXVertex>& vertices)
{
	_uploadVertices.resize(vertices.size());

	for (int index = 0; index < vertices.size(); ++index)
	{
		const PMXVertex& currentPmxVertex = vertices[index];
		UploadVertex& currentUploadVertex = _uploadVertices[index];

		currentUploadVertex.position = currentPmxVertex.position;
		currentUploadVertex.normal = currentPmxVertex.normal;
		currentUploadVertex.uv = currentPmxVertex.uv;
	}
}

void PMXActor::InitAnimation(VMDFileData& vmdFileData)
{
	for (auto& motion : vmdFileData.motions)
	{
		auto boneNode = _nodeManager.GetBoneNodeByName(motion.boneName);
		if (boneNode == nullptr)
		{
			continue;
		}

		boneNode->AddMotionKey(motion.frame,
			motion.quaternion,
			motion.translate,
			XMFLOAT2(static_cast<float>(motion.interpolation[3]) / 127.0f, static_cast<float>(motion.interpolation[7]) / 127.0f),
			XMFLOAT2(static_cast<float>(motion.interpolation[11]) / 127.0f, static_cast<float>(motion.interpolation[15]) / 127.0f));
	}

	for (VMDIK& ik : vmdFileData.iks)
	{
		for (VMDIKInfo& ikInfo : ik.ikInfos)
		{
			auto boneNode = _nodeManager.GetBoneNodeByName(ikInfo.name);
			if (boneNode == nullptr)
			{
				continue;
			}

			bool enable = ikInfo.enable;
			boneNode->AddIKkey(ik.frame, enable);
		}
	}

	_nodeManager.SortKey();

	_nodeManager.InitAnimation();
}

void PMXActor::InitPhysics(const PMXFileData& pmxFileData)
{
	PhysicsManager::ActivePhysics(false);

	for (const PMXRigidBody& pmxRigidBody : pmxFileData.rigidBodies)
	{
		RigidBody* rigidBody = new RigidBody();
		_rigidBodies.emplace_back(std::move(rigidBody));

		BoneNode* boneNode = nullptr;
		if (pmxRigidBody.boneIndex != -1)
		{
			boneNode = _nodeManager.GetBoneNodeByIndex(pmxRigidBody.boneIndex);
		}

		if (rigidBody->Init(pmxRigidBody, &_nodeManager, boneNode) == false)
		{
			OutputDebugStringA("Create Rigid Body Fail");
			continue;
		}
		PhysicsManager::AddRigidBody(rigidBody);
	}

	for (const PMXJoint& pmxJoint : pmxFileData.joints)
	{
		if (pmxJoint.rigidBodyAIndex != -1 &&
			pmxJoint.rigidBodyBIndex != -1 &&
			pmxJoint.rigidBodyAIndex != pmxJoint.rigidBodyBIndex)
		{
			Joint* joint = new Joint();
			_joints.emplace_back(std::move(joint));

			RigidBody* rigidBodyA = nullptr;
			if (_rigidBodies.size() <= pmxJoint.rigidBodyAIndex)
			{
				OutputDebugStringA("Create Joint Fail");
				continue;
			}

			rigidBodyA = _rigidBodies[pmxJoint.rigidBodyAIndex].get();

			RigidBody* rigidBodyB = nullptr;
			if (_rigidBodies.size() <= pmxJoint.rigidBodyBIndex)
			{
				OutputDebugStringA("Create Joint Fail");
				continue;
			}

			rigidBodyB = _rigidBodies[pmxJoint.rigidBodyBIndex].get();

			if (rigidBodyA->GetRigidBody()->isStaticOrKinematicObject() == true &&
				rigidBodyB->GetRigidBody()->isStaticOrKinematicObject() == true)
			{
				int d = 0;
			}

			if (joint->CreateJoint(pmxJoint, rigidBodyA, rigidBodyB) == false)
			{
				OutputDebugStringA("Create Joint Fail");
				continue;
			}

			PhysicsManager::AddJoint(joint);
		}
	}

	PhysicsManager::ActivePhysics(true);
}

void PMXActor::InitParallelVertexSkinningSetting()
{
	unsigned int threadCount = std::thread::hardware_concurrency() * 2 + 1;
	unsigned int divNum = threadCount - 1;

	_skinningRanges.resize(threadCount);
	_parallelUpdateFutures.resize(threadCount);

	unsigned int divVertexCount = _pmxFileData.vertices.size() / divNum;
	unsigned int remainder = _pmxFileData.vertices.size() % divNum;

	int startIndex = 0;
	for (int i = 0; i < _skinningRanges.size() - 1; i++)
	{
		_skinningRanges[i].startIndex = startIndex;
		_skinningRanges[i].vertexCount = divVertexCount;

		startIndex += _skinningRanges[i].vertexCount;
	}

	_skinningRanges[_skinningRanges.size() - 1].startIndex = startIndex;
	_skinningRanges[_skinningRanges.size() - 1].vertexCount = remainder;
}

void PMXActor::VertexSkinning()
{
	const int futureCount = _parallelUpdateFutures.size();

	for (int i = 0; i < futureCount; i++)
	{
		const SkinningRange& currentRange = _skinningRanges[i];
		_parallelUpdateFutures[i] = std::async(std::launch::async, [this, currentRange]()
			{
				this->VertexSkinningByRange(currentRange);
			});
	}

	for (const std::future<void>& future : _parallelUpdateFutures)
	{
		future.wait();
	}
}

void PMXActor::VertexSkinningByRange(const SkinningRange& range)
{
	for (unsigned int i = range.startIndex; i < range.startIndex + range.vertexCount; ++i)
	{
		const PMXVertex& currentVertexData = _pmxFileData.vertices[i];
		XMVECTOR position = XMLoadFloat3(&currentVertexData.position);
		XMVECTOR morphPosition = XMLoadFloat3(&_morphManager.GetMorphVertexPosition(i));

		switch (currentVertexData.weightType)
		{
		case PMXVertexWeight::BDEF1:
		{
			BoneNode* bone0 = _nodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[0]);
			XMMATRIX m0 = XMMatrixMultiply(bone0->GetInitInverseTransform(), bone0->GetGlobalTransform());
			position += morphPosition;
			position = XMVector3Transform(position, m0);
			break;
		}
		case PMXVertexWeight::BDEF2:
		{
			float weight0 = currentVertexData.boneWeights[0];
			float weight1 = 1.0f - weight0;

			BoneNode* bone0 = _nodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[0]);
			BoneNode* bone1 = _nodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[1]);

			XMMATRIX m0 = XMMatrixMultiply(bone0->GetInitInverseTransform(), bone0->GetGlobalTransform());
			XMMATRIX m1 = XMMatrixMultiply(bone1->GetInitInverseTransform(), bone1->GetGlobalTransform());

			XMMATRIX mat = m0 * weight0 + m1 * weight1;
			position += morphPosition;
			position = XMVector3Transform(position, mat);
			break;
		}
		case PMXVertexWeight::BDEF4:
		{
			float weight0 = currentVertexData.boneWeights[0];
			float weight1 = currentVertexData.boneWeights[1];
			float weight2 = currentVertexData.boneWeights[2];
			float weight3 = currentVertexData.boneWeights[3];

			BoneNode* bone0 = _nodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[0]);
			BoneNode* bone1 = _nodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[1]);
			BoneNode* bone2 = _nodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[2]);
			BoneNode* bone3 = _nodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[3]);

			XMMATRIX m0 = XMMatrixMultiply(bone0->GetInitInverseTransform(), bone0->GetGlobalTransform());
			XMMATRIX m1 = XMMatrixMultiply(bone1->GetInitInverseTransform(), bone1->GetGlobalTransform());
			XMMATRIX m2 = XMMatrixMultiply(bone2->GetInitInverseTransform(), bone2->GetGlobalTransform());
			XMMATRIX m3 = XMMatrixMultiply(bone3->GetInitInverseTransform(), bone3->GetGlobalTransform());

			XMMATRIX mat = m0 * weight0 + m1 * weight1 + m2 * weight2 + m3 * weight3;
			position += morphPosition;
			position = XMVector3Transform(position, mat);
			break;
		}
		case PMXVertexWeight::SDEF:
		{
			float w0 = currentVertexData.boneWeights[0];
			float w1 = 1.0f - w0;

			XMVECTOR sdefc = XMLoadFloat3(&currentVertexData.sdefC);
			XMVECTOR sdefr0 = XMLoadFloat3(&currentVertexData.sdefR0);
			XMVECTOR sdefr1 = XMLoadFloat3(&currentVertexData.sdefR1);

				//rw = sdefr0 * w0 + sdefr1 * w1
				//r0 = sdefc + sdefr0 - rw
				//r1 = sdefc + sdefr1 - rw

			XMVECTOR rw = XMVectorAdd(sdefr0 * w0, sdefr1 * w1);
			XMVECTOR r0 = XMVectorSubtract(XMVectorAdd(sdefc, sdefr0), rw);
			XMVECTOR r1 = XMVectorSubtract(XMVectorAdd(sdefc, sdefr1), rw);

				// cr0 = (sdefc + r0) * 0.5f
				// cr1 = (sdefc + r1) * 0.5f

			XMVECTOR cr0 = XMVectorAdd(sdefc, r0) * 0.5f;
			XMVECTOR cr1 = XMVectorAdd(sdefc, r1) * 0.5f;

			BoneNode* bone0 = _nodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[0]);
			BoneNode* bone1 = _nodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[1]);

			XMVECTOR q0 = XMQuaternionRotationMatrix(bone0->GetGlobalTransform());
			XMVECTOR q1 = XMQuaternionRotationMatrix(bone1->GetGlobalTransform());

			XMMATRIX m0 = XMMatrixMultiply(bone0->GetInitInverseTransform(), bone0->GetGlobalTransform());
			XMMATRIX m1 = XMMatrixMultiply(bone1->GetInitInverseTransform(), bone1->GetGlobalTransform());

			XMMATRIX rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(q0, q1, w1));

			position += morphPosition;

				// XMVector3Transform(position - sdefc, rotation) + XMVector3Transform(cr0, m0) * w0 + XMVector3Transform(cr1, m1) * w1

			XMVECTOR a = XMVector3Transform(XMVectorSubtract(position, sdefc), rotation);
			XMVECTOR b = XMVector3Transform(cr0, m0) * w0;
			XMVECTOR c = XMVector3Transform(cr1, m1) * w1;

			position = XMVectorAdd(XMVectorAdd(a, b), c);
			XMVECTOR normal = XMLoadFloat3(&currentVertexData.normal);
			normal = XMVector3Transform(normal, rotation);
			XMStoreFloat3(&_uploadVertices[i].normal, normal);
			break;
		}
		case PMXVertexWeight::QDEF:
		{
			BoneNode* bone0 = _nodeManager.GetBoneNodeByIndex(currentVertexData.boneIndices[0]);
			XMMATRIX m0 = XMMatrixMultiply(bone0->GetInitInverseTransform(), bone0->GetGlobalTransform());
			position += morphPosition;

			position = XMVector3Transform(position, m0);

			break;
		}
		default:
			break;
		}

		XMStoreFloat3(&_uploadVertices[i].position, position);

		const XMFLOAT4& morphUV = _morphManager.GetMorphUV(i);
		const XMFLOAT2& originalUV = _uploadVertices[i].uv;
		_uploadVertices[i].uv = XMFLOAT2(originalUV.x + morphUV.x, originalUV.y + morphUV.y);
	}
}

void PMXActor::MorphMaterial()
{
	size_t bufferSize = sizeof(MaterialForShader);
	bufferSize = (bufferSize + 0xff) & ~0xff;

	char* mappedMaterialPtr = _mappedMaterial;

	for (int i = 0; i < _loadedMaterial.size(); i++)
	{
		LoadMaterial& material = _loadedMaterial[i];

		MaterialForShader* uploadMat = reinterpret_cast<MaterialForShader*>(mappedMaterialPtr);

		XMVECTOR diffuse = XMLoadFloat4(&material.diffuse);
		XMVECTOR specular = XMLoadFloat3(&material.specular);
		XMVECTOR ambient = XMLoadFloat3(&material.ambient);

		const MaterialMorphData& morphMaterial = _morphManager.GetMorphMaterial(i);
		float weight = morphMaterial.weight;

		XMVECTOR morphDiffuse = XMLoadFloat4(&morphMaterial.diffuse);
		XMVECTOR morphSpecular = XMLoadFloat3(&morphMaterial.specular);
		XMVECTOR morphAmbient = XMLoadFloat3(&morphMaterial.ambient);

		XMFLOAT4 resultDiffuse;
		XMFLOAT3 resultSpecular;
		XMFLOAT3 resultAmbient;

		if (morphMaterial.opType == PMXMorph::MaterialMorph::OpType::Add)
		{
			XMStoreFloat4(&resultDiffuse, XMVectorAdd(diffuse, morphDiffuse * weight));
			XMStoreFloat3(&resultSpecular, XMVectorAdd(specular, morphSpecular * weight));
			XMStoreFloat3(&resultAmbient, XMVectorAdd(ambient, morphAmbient * weight));
			uploadMat->specularPower += morphMaterial.specularPower * weight;
		}
		else
		{
			morphDiffuse = XMVectorMultiply(diffuse, morphDiffuse);
			morphSpecular = XMVectorMultiply(specular, morphSpecular);
			morphAmbient = XMVectorMultiply(ambient, morphAmbient);

			XMStoreFloat4(&resultDiffuse, XMVectorLerp(diffuse, morphDiffuse, weight));
			XMStoreFloat3(&resultSpecular, XMVectorLerp(specular, morphSpecular, weight));
			XMStoreFloat3(&resultAmbient, XMVectorLerp(ambient, morphAmbient, weight));
			uploadMat->specularPower = MathUtil::Lerp(material.specularPower, material.specularPower * morphMaterial.specularPower, weight);
		}

		uploadMat->diffuse = resultDiffuse;
		uploadMat->specular = resultSpecular;
		uploadMat->ambient = resultAmbient;

		mappedMaterialPtr += bufferSize;
	}
}

void PMXActor::MorphBone()
{
	const std::vector<BoneNode*>& allNodes = _nodeManager.GetAllNodes();

	for (BoneNode* boneNode : allNodes)
	{
		BoneMorphData morph = _morphManager.GetMorphBone(boneNode->GetBoneIndex());
		boneNode->SetMorphPosition(MathUtil::Lerp(XMFLOAT3(0.f, 0.f, 0.f), morph.position, morph.weight));

		XMVECTOR animateRotation = XMQuaternionRotationMatrix(boneNode->GetAnimateRotation());
		XMVECTOR morphRotation = XMLoadFloat4(&morph.quaternion);

		animateRotation = XMQuaternionSlerp(animateRotation, morphRotation, morph.weight);
		boneNode->SetAnimateRotation(XMMatrixRotationQuaternion(animateRotation));
	}
}

void PMXActor::ResetPhysics()
{
	for (auto& rigidBody : _rigidBodies)
	{
		rigidBody->SetActive(false);
		rigidBody->ResetTransform();
	}

	for (auto& rigidBody : _rigidBodies)
	{
		rigidBody->ReflectGlobalTransform();
	}

	for (auto& rigidBody : _rigidBodies)
	{
		rigidBody->CalcLocalTransform();
	}

	const auto& nodes = _nodeManager.GetAllNodes();
	for (const auto& node : nodes)
	{
		if (node->GetParentBoneNode() == nullptr)
		{
			node->UpdateGlobalTransform();
		}
	}

	btDiscreteDynamicsWorld* world = PhysicsManager::GetDynamicsWorld();
	for (auto& rigidBody : _rigidBodies)
	{
		rigidBody->Reset(world);
	}
}

void PMXActor::UpdatePhysicsAnimation(DWORD elapse)
{
	for (auto& rigidBody : _rigidBodies)
	{
		rigidBody->SetActive(true);
	}

	//_physicsManager.Update(elapse);

	for (auto& rigidBody : _rigidBodies)
	{
		rigidBody->ReflectGlobalTransform();
	}

	for (auto& rigidBody : _rigidBodies)
	{
		rigidBody->CalcLocalTransform();
	}

	const auto& nodes = _nodeManager.GetAllNodes();
	for (const auto& node : nodes)
	{
		if (node->GetParentBoneNode() == nullptr)
		{
			node->UpdateGlobalTransform();
		}
	}
}

