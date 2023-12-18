#include "PMXActor.h"
#include <array>
#include <bitset>
#include "Dx12Wrapper.h"
#include "PMXRenderer.h"
#include "srtconv.h"
#include <d3dx12.h>

using namespace std;

constexpr float epsilon = 0.0005f;

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

	LoadVertexData(_pmxFileData.vertices);

	result = LoadVMDFile(L"VMD\\ラビットホール.vmd", _vmdFileData);
	if (result == false)
	{
		return false;
	}

	_transform.world = XMMatrixIdentity() * XMMatrixTranslation(0.0f, 0.0f, 0.0f);

	InitParallelVertexSkinningSetting();

	InitAnimation(_vmdFileData);
	InitBoneNode(_pmxFileData.bones);

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

void PMXActor::UpdateAnimation()
{
	if (_startTime <= 0)
	{
		_startTime = timeGetTime();
	}

	DWORD elapsedTime = timeGetTime() - _startTime;
	unsigned int frameNo = 30 * (elapsedTime / 1000.0f);

	if (frameNo > _duration)
	{
		_startTime = timeGetTime();
		frameNo = 0;
	}

	//std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());
	for (auto& bone : _boneNodeByIdx)
	{
		XMFLOAT3& position = bone->position;
		_boneMatrices[bone->boneIndex] = XMMatrixTranslationFromVector(XMLoadFloat3(&position));
	}

	for (auto& keyByBoneName : _animationKeyMap)
	{
		const std::wstring& name = keyByBoneName.first;
		std::vector<VMDKey>& keyList = keyByBoneName.second;

		auto itBondNode = _boneNodeByName.find(name);
		if (itBondNode == _boneNodeByName.end())
		{
			continue;
		}

		BoneNode& node = itBondNode->second;

		auto rit = std::find_if(keyList.rbegin(), keyList.rend(),
			[frameNo](const VMDKey& key)
			{
				return key.frameNo <= frameNo;
			});

		XMMATRIX rotation;
		XMVECTOR position = XMLoadFloat3(&rit->offset);

		auto iterator = rit.base();

		if (iterator != keyList.end())
		{
			float t = static_cast<float>(frameNo - rit->frameNo) / static_cast<float>(iterator->frameNo - rit->frameNo);

			t = GetYFromXOnBezier(t, iterator->p1, iterator->p2, 12);

			rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(rit->quaternion, iterator->quaternion, t));
			position = XMVectorLerp(position, XMLoadFloat3(&iterator->offset), t);
		}
		else
		{
			rotation = XMMatrixRotationQuaternion(rit->quaternion);
		}

		XMFLOAT3& startPosition = node.position;

		XMMATRIX mat = 
			XMMatrixTranslation(-startPosition.x, -startPosition.y, -startPosition.z)
			* rotation
			* XMMatrixTranslation(startPosition.x, startPosition.y, startPosition.z);

		_boneMatrices[node.boneIndex] = rotation * XMMatrixTranslationFromVector(position + XMLoadFloat3(&startPosition));
		//_boneLocalMatrices[node.boneIndex] = XMMatrixTranslation(-startPosition.x, -startPosition.y, -startPosition.z);
	}

	BoneNode* centerNode = _boneNodeByIdx[0];
	RecursiveMatrixMultiply(centerNode, XMMatrixIdentity());

	VertexSkinning();

	std::copy(_uploadVertices.begin(), _uploadVertices.end(), _mappedVertex);
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
		_animationKeyMap[motion.boneName].emplace_back(
			motion.frame,
			XMLoadFloat4(&motion.quaternion),
			motion.translate,
			XMFLOAT2(static_cast<float>(motion.interpolation[3]) / 127.0f, static_cast<float>(motion.interpolation[7]) / 127.0f),
			XMFLOAT2(static_cast<float>(motion.interpolation[11]) / 127.0f, static_cast<float>(motion.interpolation[15]) / 127.0f));
	}

	for (auto& keys : _animationKeyMap)
	{
		std::sort(keys.second.begin(), keys.second.end(),
			[](const VMDKey& left, const VMDKey& right)
			{
				return left.frameNo <= right.frameNo;
			});

		for (auto& key : keys.second)
		{
			_duration = std::max<unsigned int>(_duration, key.frameNo);
		}
	}
}

void PMXActor::InitBoneNode(std::vector<PMXBone>& bones)
{
	_boneNodeNames.resize(bones.size());
	_boneNodeByIdx.resize(bones.size());
	_boneLocalMatrices.resize(bones.size());

	for (int index = 0; index < bones.size(); index++)
	{
		PMXBone& currentBoneData = bones[index];
		BoneNode& currentBoneNode = _boneNodeByName[currentBoneData.name];

		currentBoneNode.boneIndex = index;

		currentBoneNode.name = currentBoneData.name;
		currentBoneNode.englishName = currentBoneData.englishName;

		currentBoneNode.position = currentBoneData.position;
		currentBoneNode.parentBoneIndex = currentBoneData.parentBoneIndex;
		currentBoneNode.deformDepth = currentBoneData.deformDepth;

		currentBoneNode.boneFlag = currentBoneData.boneFlag;

		currentBoneNode.positionOffset = currentBoneData.positionOffset;
		currentBoneNode.linkBoneIndex = currentBoneData.linkBoneIndex;

		currentBoneNode.appendBoneIndex = currentBoneData.appendBoneIndex;
		currentBoneNode.appendWeight = currentBoneData.appendWeight;

		currentBoneNode.fixedAxis = currentBoneData.fixedAxis;
		currentBoneNode.localXAxis = currentBoneData.localXAxis;
		currentBoneNode.localZAxis = currentBoneData.localZAxis;

		currentBoneNode.keyValue = currentBoneData.keyValue;

		currentBoneNode.ikTargetBoneIndex = currentBoneData.ikTargetBoneIndex;
		currentBoneNode.ikIterationCount = currentBoneData.ikIterationCount;
		currentBoneNode.ikLimit = currentBoneData.ikLimit;

		_boneNodeByIdx[index] = &currentBoneNode;
		_boneNodeNames[index] = currentBoneNode.name;
	}

	for (auto& bone : bones)
	{
		if (bone.parentBoneIndex == 65535 || bone.parentBoneIndex >= bones.size())
		{
			continue;
		}

		BoneNode& curNode = _boneNodeByName[bone.name];

		std::wstring parentBoneName = _boneNodeNames[bone.parentBoneIndex];
		BoneNode& parentNode = _boneNodeByName[parentBoneName];

		_boneNodeByName[parentBoneName].childrenNode.push_back(&curNode);
		curNode.parentNode = &parentNode;

		XMFLOAT3& curBonePosition = _pmxFileData.bones[curNode.boneIndex].position;

		XMVECTOR pos = XMLoadFloat3(&curBonePosition);
		XMVECTOR parentPos = XMLoadFloat3(&_pmxFileData.bones[bone.parentBoneIndex].position);

		XMStoreFloat3(&curNode.position, pos - parentPos);

		_boneLocalMatrices[curNode.boneIndex] = XMMatrixTranslation(-curBonePosition.x, -curBonePosition.y, -curBonePosition.z);
	}
}

float PMXActor::GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n)
{
	if (a.x == a.y && b.x == b.y)
	{
		return x;
	}

	float t = x;
	const float k0 = 1 + 3 * a.x - 3 * b.x;
	const float k1 = 3 * b.x - 6 * a.x;
	const float k2 = 3 * a.x;

	for (int i = 0; i < n; ++i)
	{
		auto ft = k0 * t * t * t + k1 * t * t + k2 * t - x;

		if (ft <= epsilon && ft >= -epsilon)
		{
			break;
		}

		t -= ft / 2;
	}

	auto r = 1 - t;
	return t * t * t + 3 * t * t * r * b.y + 3 * t * r * r * a.y;
}

void PMXActor::RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat)
{
	_boneMatrices[node->boneIndex] *= mat;

	for (auto& childNode : node->childrenNode)
	{
		RecursiveMatrixMultiply(childNode, _boneMatrices[node->boneIndex]);
	}
}

void PMXActor::InitParallelVertexSkinningSetting()
{
	unsigned int threadCount = std::thread::hardware_concurrency();
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

		switch (currentVertexData.weightType)
		{
		case PMXVertexWeight::BDEF1:
		{
			XMMATRIX m0 = XMMatrixMultiply(_boneLocalMatrices[currentVertexData.boneIndices[0]], _boneMatrices[currentVertexData.boneIndices[0]]);
			position = XMVector3Transform(position, m0);
			break;
		}
		case PMXVertexWeight::BDEF2:
		{
			float weight0 = currentVertexData.boneWeights[0];
			float weight1 = 1.0f - weight0;
			XMMATRIX m0 = XMMatrixMultiply(_boneLocalMatrices[currentVertexData.boneIndices[0]], _boneMatrices[currentVertexData.boneIndices[0]]);
			XMMATRIX m1 = XMMatrixMultiply(_boneLocalMatrices[currentVertexData.boneIndices[1]], _boneMatrices[currentVertexData.boneIndices[1]]);
			XMMATRIX mat = m0 * weight0 + m1 * weight1;
			position = XMVector3Transform(position, mat);
			break;
		}
		case PMXVertexWeight::BDEF4:
		{
			float weight0 = currentVertexData.boneWeights[0];
			float weight1 = currentVertexData.boneWeights[1];
			float weight2 = currentVertexData.boneWeights[2];
			float weight3 = currentVertexData.boneWeights[3];
			XMMATRIX m0 = XMMatrixMultiply(_boneLocalMatrices[currentVertexData.boneIndices[0]], _boneMatrices[currentVertexData.boneIndices[0]]);
			XMMATRIX m1 = XMMatrixMultiply(_boneLocalMatrices[currentVertexData.boneIndices[1]], _boneMatrices[currentVertexData.boneIndices[1]]);
			XMMATRIX m2 = XMMatrixMultiply(_boneLocalMatrices[currentVertexData.boneIndices[2]], _boneMatrices[currentVertexData.boneIndices[2]]);
			XMMATRIX m3 = XMMatrixMultiply(_boneLocalMatrices[currentVertexData.boneIndices[3]], _boneMatrices[currentVertexData.boneIndices[3]]);
			XMMATRIX mat = m0 * weight0 + m1 * weight1 + m2 * weight2 + m3 * weight3;
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

			XMVECTOR rw = sdefr0 * w0 + sdefr1 * w1;
			XMVECTOR r0 = sdefc + sdefr0 - rw;
			XMVECTOR r1 = sdefc + sdefr1 - rw;

			XMVECTOR cr0 = (sdefc + r0) * 0.5f;
			XMVECTOR cr1 = (sdefc + r1) * 0.5f;

			XMVECTOR q0 = XMQuaternionRotationMatrix(_boneMatrices[currentVertexData.boneIndices[0]]);
			XMVECTOR q1 = XMQuaternionRotationMatrix(_boneMatrices[currentVertexData.boneIndices[1]]);

			XMMATRIX m0 = XMMatrixMultiply(_boneLocalMatrices[currentVertexData.boneIndices[0]], _boneMatrices[currentVertexData.boneIndices[0]]);
			XMMATRIX m1 = XMMatrixMultiply(_boneLocalMatrices[currentVertexData.boneIndices[1]], _boneMatrices[currentVertexData.boneIndices[1]]);

			XMMATRIX rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(q0, q1, w1));

			position = XMVector3Transform(position - sdefc, rotation) + XMVector3Transform(cr0, m0) * w0 + XMVector3Transform(cr1, m1) * w1;
			XMVECTOR normal = XMLoadFloat3(&currentVertexData.normal);
			normal = XMVector3Transform(normal, rotation);
			XMStoreFloat3(&_uploadVertices[i].normal, normal);
			break;
		}
		case PMXVertexWeight::QDEF:
		{
			XMMATRIX bone0 = _boneMatrices[currentVertexData.boneIndices[0]];
			position = XMVector3Transform(position, bone0);
			break;
		}
		default:
			break;
		}

		XMStoreFloat3(&_uploadVertices[i].position, position);
	}
}

