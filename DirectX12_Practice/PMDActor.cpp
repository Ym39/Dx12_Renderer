#include "PMDActor.h"
#include "PMDRenderer.h"
#include "Dx12Wrapper.h"
#include "Utill.h"
#include <d3dx12.h>

using namespace Microsoft::WRL;
using namespace std;

#pragma comment(lib,"winmm.lib")

constexpr float epsilon = 0.0005f;

void* PMDActor::Transform::operator new(size_t size) {
	return _aligned_malloc(size, 16);
}

PMDActor::PMDActor(const char* filepath, Dx12Wrapper& dx):
	_angle(0.0f)
{
	_transform.world = XMMatrixIdentity() * XMMatrixRotationY(-45.0f) * XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	LoadPMDFile(filepath, dx);
	LoadVMDFile("Model/squat.vmd");
	CreateTransformView(dx);
	CreateMaterialData(dx);
	CreateMaterialAndTextureView(dx);
}

PMDActor::~PMDActor()
{
}

void PMDActor::Update()
{
	//_angle += 0.005f;
	_mappedMatrices[0] = XMMatrixRotationY(_angle) * XMMatrixTranslation(_position.x, _position.y, _position.z);

	//MotionUpdate();
}

void PMDActor::PlayAnimation()
{
	_startTime = timeGetTime();
}

void PMDActor::Draw(Dx12Wrapper& dx, bool isShadow = false)
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

	auto cbvsrvIncSize = dx.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;

	if (isShadow == true)
	{
		dx.CommandList()->DrawIndexedInstanced(_indexNum, 1, 0, 0, 0);
	}
	else
	{
		for (auto& m : _materials)
		{
			dx.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
			dx.CommandList()->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
			materialH.ptr += cbvsrvIncSize;
			idxOffset += m.indicesNum;
		}
	}
}

void PMDActor::SetPosition(float x, float y, float z)
{
	_position.x = x;
	_position.y = y;
	_position.z = z;
}

HRESULT PMDActor::LoadPMDFile(const char* path, Dx12Wrapper& dx)
{
	struct PMDHeader
	{
		float version;
		char modle_name[20];
		char comment[256];
	};

	char signature[3] = {};
	PMDHeader pmdHeader;

	std::string strModelPath = path;

	auto fp = fopen(strModelPath.c_str(), "rb");
	if (fp == nullptr)
	{
		assert(0);
		return ERROR_FILE_NOT_FOUND;
	}

	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdHeader, sizeof(pmdHeader), 1, fp);

	fread(&_vertNum, sizeof(_vertNum), 1, fp);

#pragma pack(1)

	struct PMDMaterial
	{
		XMFLOAT3 diffuse;
		float alpha;
		float specularity;
		XMFLOAT3 specular;
		XMFLOAT3 ambient;
		unsigned char toonIdx;
		unsigned char edgeFlg;

		unsigned int indicesNum;

		char texFilePath[20];
	};

#pragma pack()

	constexpr size_t pmdvertex_size = 38;
	std::vector<unsigned char> vertices(_vertNum * pmdvertex_size);
	fread(vertices.data(), vertices.size(), 1, fp);

	fread(&_indexNum, sizeof(_indexNum), 1, fp);

	D3D12_HEAP_PROPERTIES heapprop = {};

	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};

	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = vertices.size() * sizeof(vertices[0]);
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	auto result = dx.Device()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(_vb.ReleaseAndGetAddressOf()));

	unsigned char* vertMap = nullptr;

	result = _vb->Map(0, nullptr, (void**)&vertMap);

	std::copy(std::begin(vertices), std::end(vertices), vertMap);

	_vb->Unmap(0, nullptr);

	_vbView.BufferLocation = _vb->GetGPUVirtualAddress();
	_vbView.SizeInBytes = vertices.size();
	_vbView.StrideInBytes = pmdvertex_size;

	std::vector<unsigned short> indices;

	indices.resize(_indexNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

	resdesc.Width = indices.size() * sizeof(indices[0]);

	result = dx.Device()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(_ib.ReleaseAndGetAddressOf()));

	unsigned short* mappedIdx = nullptr;
	_ib->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(indices), std::end(indices), mappedIdx);
	_ib->Unmap(0, nullptr);

	_ibView.BufferLocation = _ib->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = indices.size() * sizeof(indices[0]);

	unsigned int materialNum;
	fread(&materialNum, sizeof(materialNum), 1, fp);

	_materials.resize(materialNum);
	_textureResources.resize(materialNum);
	_sphResources.resize(materialNum);
	_spaResources.resize(materialNum);
	_toonResources.resize(materialNum);

	std::vector<PMDMaterial> pmdMaterials(materialNum);

	fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);

	for (int i = 0; i < pmdMaterials.size(); ++i)
	{
		_materials[i].indicesNum = pmdMaterials[i].indicesNum;
		_materials[i].material.diffuse = pmdMaterials[i].diffuse;
		_materials[i].material.alpha = pmdMaterials[i].alpha;
		_materials[i].material.specular = pmdMaterials[i].specular;
		_materials[i].material.specularity = pmdMaterials[i].specularity;
		_materials[i].material.ambient = pmdMaterials[i].ambient;
		_materials[i].additional.toonIdx = pmdMaterials[i].toonIdx;
	}

	for (int i = 0; i < pmdMaterials.size(); ++i)
	{
		string toonFilePath = "toon/";

		char toonFileName[16];

		sprintf(toonFileName, "toon%02d.bmp", pmdMaterials[i].toonIdx + 1);

		toonFilePath += toonFileName;

		_toonResources[i] = dx.GetTextureByPath(toonFilePath.c_str());

		if (strlen(pmdMaterials[i].texFilePath) == 0)
		{
			_textureResources[i] = nullptr;
		}

		std::string texFileName = pmdMaterials[i].texFilePath;
		std::string sphFileName = "";
		std::string spaFileName = "";

		if (std::count(texFileName.begin(), texFileName.end(), '*') > 0)
		{
			auto namepair = SplitFileName(texFileName);

			if (GetExtension(namepair.first) == "sph")
			{
				texFileName = namepair.second;
				sphFileName = namepair.first;
			}
			else if (GetExtension(namepair.first) == "spa")
			{
				texFileName = namepair.first;
				sphFileName = namepair.first;
			}
			else
			{
				texFileName = namepair.first;
				if (GetExtension(namepair.second) == "sph") {
					sphFileName = namepair.second;
				}
				else if (GetExtension(namepair.second) == "spa") {
					spaFileName = namepair.second;
				}
			}
		}
		else
		{
			if (GetExtension(pmdMaterials[i].texFilePath) == "sph")
			{
				sphFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else if (GetExtension(pmdMaterials[i].texFilePath) == "spa")
			{
				spaFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else
			{
				texFileName = pmdMaterials[i].texFilePath;
			}
		}

		if (texFileName != "")
		{
			auto texFilePath = GetTexturePathFromModelAndTexPath(strModelPath, texFileName.c_str());
			_textureResources[i] = dx.GetTextureByPath(texFilePath.c_str());
		}
		if (sphFileName != "")
		{
			auto sphFilePath = GetTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
			_sphResources[i] = dx.GetTextureByPath(sphFilePath.c_str());
		}
		if (spaFileName != "")
		{
			auto spaFilePath = GetTexturePathFromModelAndTexPath(strModelPath, spaFileName.c_str());
			_spaResources[i] = dx.GetTextureByPath(spaFilePath.c_str());
		}
	}

#pragma pack(1)
	struct PMDBone
	{
		char boneName[20];
		unsigned short parentNo;
		unsigned short nextNo;
		unsigned char type;
		unsigned short ikBoneNo;
		XMFLOAT3 pos;
	};
#pragma pack()

	//본
	unsigned short boneNum = 0;
	fread(&boneNum, sizeof(boneNum), 1, fp);

	std::vector<PMDBone> pmdBones(boneNum);
	fread(pmdBones.data(), sizeof(PMDBone), boneNum, fp);

	//IK
	uint16_t ikNum = 0;
	fread(&ikNum, sizeof(ikNum), 1, fp);

	_ikData.resize(ikNum);
	for (auto& ik : _ikData)
	{
		fread(&ik.boneIdx, sizeof(ik.boneIdx), 1, fp);
		fread(&ik.targetIdx, sizeof(ik.targetIdx), 1, fp);
		uint8_t chainLen = 0;
		fread(&chainLen, sizeof(chainLen), 1, fp);
		ik.nodeIdx.resize(chainLen);
		fread(&ik.iterations, sizeof(ik.iterations), 1, fp);
		fread(&ik.limit, sizeof(ik.limit), 1, fp);
		if (chainLen == 0)continue;
		fread(ik.nodeIdx.data(), sizeof(ik.nodeIdx[0]), chainLen, fp);
	}

	//IK 디버깅 용
	auto getNameFromIdx = [&](uint16_t idx)->string
	{
		auto it = find_if(_boneNodeTable.begin(), _boneNodeTable.end(),
			[idx](const std::pair<string, BoneNode>& obj)
			{
				return obj.second.boneIdx == idx;
			});

		if (it != _boneNodeTable.end())
		{
			return it->first;
		}
		else
		{
			return "";
		}
	};

	for (auto& ik : _ikData)
	{
		std::ostringstream oss;
		oss << "IK number =" << ik.boneIdx << ":" << getNameFromIdx(ik.boneIdx) << endl;

		for (auto& node : ik.nodeIdx)
		{
			oss << "Node bone =" << node << ":" << getNameFromIdx(node) << endl;
		}

		OutputDebugStringA(oss.str().c_str());
	}


	_boneNameArray.resize(pmdBones.size());
	_boneNodeAddressArray.resize(pmdBones.size());

	_kneeIdxes.clear();
	for (int idx = 0; idx < pmdBones.size(); ++idx)
	{
		auto& pb = pmdBones[idx];
		auto& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
		node.boneType = pb.type;
		node.parentBone = pb.parentNo;
		node.ikParentBone = pb.ikBoneNo;

		_boneNameArray[idx] = pb.boneName;
		_boneNodeAddressArray[idx] = &node;

		string boneName = pb.boneName;

		if (boneName.find("궿궡") != std::string::npos)
		{
			_kneeIdxes.emplace_back(idx);
		}
	}

	for (auto& pb : pmdBones)
	{
		if (pb.parentNo >= pmdBones.size())
		{
			continue;
		}

		auto parentName = _boneNameArray[pb.parentNo];
		_boneNodeTable[parentName].children.emplace_back(&_boneNodeTable[pb.boneName]);
	}

	for (auto& boneNode : _boneNodeTable)
	{
		_boneNodeTableByIdx[boneNode.second.boneIdx] = boneNode.second;
	}

	_boneMatrices.resize(pmdBones.size());

	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	fclose(fp);

	return S_OK;
}

void PMDActor::LoadVMDFile(const char* filepath)
{
	auto fp = fopen(filepath, "rb");
	fseek(fp, 50, SEEK_SET);
	unsigned int keyframeNum = 0;
	fread(&keyframeNum, sizeof(keyframeNum), 1, fp);

	struct VMDKeyFrame 
	{
		char boneName[15]; 
		unsigned int frameNo; 
		XMFLOAT3 location; 
		XMFLOAT4 quaternion; 
		unsigned char bezier[64]; 
	};

	vector<VMDKeyFrame> keyframes(keyframeNum);
	for (auto& keyframe : keyframes)
	{
		fread(keyframe.boneName, sizeof(keyframe.boneName), 1, fp);
		fread(&keyframe.frameNo, sizeof(keyframe.frameNo) +
			sizeof(keyframe.location) +
			sizeof(keyframe.quaternion) +
			sizeof(keyframe.bezier), 1, fp);
	}

#pragma pack(1)
	struct VMDMorph 
	{
		char name[15];
		uint32_t frameNo;
		float weight;
	};
#pragma pack()

	uint32_t morphCount = 0;
	fread(&morphCount, sizeof(morphCount), 1, fp);
	vector<VMDMorph> morphs(morphCount);
	fread(morphs.data(), sizeof(VMDMorph), morphCount, fp);

#pragma pack(1)
	struct VMDCamera 
	{
		uint32_t frameNo; 
		float distance;
		XMFLOAT3 pos;
		XMFLOAT3 eulerAngle; 
		uint8_t Interpolation[24];
		uint32_t fov;
		uint8_t persFlg; 
	};
#pragma pack()
	uint32_t vmdCameraCount = 0;
	fread(&vmdCameraCount, sizeof(vmdCameraCount), 1, fp);
	vector<VMDCamera> cameraData(vmdCameraCount);
	fread(cameraData.data(), sizeof(VMDCamera), vmdCameraCount, fp);


	struct VMDLight 
	{
		uint32_t frameNo;
		XMFLOAT3 rgb;
		XMFLOAT3 vec; 
	};

	uint32_t vmdLightCount = 0;
	fread(&vmdLightCount, sizeof(vmdLightCount), 1, fp);
	vector<VMDLight> lights(vmdLightCount);
	fread(lights.data(), sizeof(VMDLight), vmdLightCount, fp);

#pragma pack(1)
	struct VMDSelfShadow 
	{
		uint32_t frameNo;
		uint8_t mode; 
		float distance; 
	};
#pragma pack()

	uint32_t selfShadowCount = 0;
	fread(&selfShadowCount, sizeof(selfShadowCount), 1, fp);
	vector<VMDSelfShadow> selfShadowData(selfShadowCount);
	fread(selfShadowData.data(), sizeof(VMDSelfShadow), selfShadowCount, fp);

	uint32_t ikSwitchCount = 0;
	fread(&ikSwitchCount, sizeof(ikSwitchCount), 1, fp);
	
	_ikEnableData.resize(ikSwitchCount);
	for (auto& ikEnable : _ikEnableData)
	{
		fread(&ikEnable.frameNo, sizeof(ikEnable.frameNo), 1, fp);
		uint8_t visibleFlg = 0;
		fread(&visibleFlg, sizeof(visibleFlg), 1, fp);
		uint32_t ikBoneCount = 0;
		fread(&ikBoneCount, sizeof(ikBoneCount), 1, fp);
		for (int i = 0; i < ikBoneCount; ++i)
		{
			char ikBoneName[20];
			fread(ikBoneName, _countof(ikBoneName), 1, fp);
			uint8_t flg = 0;
			fread(&flg, sizeof(flg), 1, fp);
			ikEnable.ikEnableTable[ikBoneName] = flg;
		}
	}
	fclose(fp);

	for (auto& vmdMotion : keyframes)
	{
		_motionData[vmdMotion.boneName].
			emplace_back(Motion(vmdMotion.frameNo,
				XMLoadFloat4(&vmdMotion.quaternion),
				vmdMotion.location,
				XMFLOAT2((float)vmdMotion.bezier[3] / 127.0f, (float)vmdMotion.bezier[7] / 127.0f),
				XMFLOAT2((float)vmdMotion.bezier[11] / 127.0f, (float)vmdMotion.bezier[15] / 127.0f)));
	}

	for (auto& motions : _motionData)
	{
		std::sort(
			motions.second.begin(), motions.second.end(),
			[](const Motion& lval, const Motion& rval)
			{
				return lval.frameNo <= rval.frameNo;
			});

		for (auto& motion : motions.second)
		{
			_duration = std::max<unsigned int>(_duration, motion.frameNo);
		}
	}
}

void PMDActor::LoadVMDIKFile(const char* filepath)
{
	auto fp = fopen(filepath, "rb");
	fseek(fp, 50, SEEK_SET);

	unsigned int motionDataNum = 0;
	fread(&motionDataNum, sizeof(motionDataNum), 1, fp);

	struct VMDMotion
	{
		char boneName[15];
		unsigned int frameNo;
		XMFLOAT3 location;
		XMFLOAT4 quaternion;
		unsigned char bezier[64];
	};

	std::vector<VMDMotion> vmdMotionData(motionDataNum);
	for (auto& motion : vmdMotionData)
	{
		fread(motion.boneName, sizeof(motion.boneName), 1, fp);
		fread(&motion.frameNo,
			sizeof(motion.frameNo)
			+ sizeof(motion.location)
			+ sizeof(motion.quaternion)
			+ sizeof(motion.bezier),
			1, fp);
	}

#pragma pack(1)
	struct VMDMorph
	{
		char name[15];
		uint32_t frameNo;
		float weight;
	};
#pragma pack()

	uint32_t morphCount = 0;
	fread(&morphCount, sizeof(morphCount), 1, fp);

	std::vector<VMDMorph> morphs(morphCount);
	fread(morphs.data(), sizeof(morphCount), 1, fp);

#pragma pack(1)
	struct VMDCamera
	{
		uint32_t frameNo;
		float distance;
		XMFLOAT3 pos;
		XMFLOAT3 eulerAngle;
		uint8_t Interpolation[24];
		uint32_t fov;
		uint8_t persFlg;
	};
#pragma pack()
	uint32_t vmdCameraCount = 0;
	fread(&vmdCameraCount, sizeof(vmdCameraCount), 1, fp);
	vector<VMDCamera> cameraData(vmdCameraCount);
	fread(cameraData.data(), sizeof(VMDCamera), vmdCameraCount, fp);

	struct VMDLight
	{
		uint32_t frameNo;
		XMFLOAT3 rgb;
		XMFLOAT3 vec;
	};

	uint32_t vmdLightCount = 0;
	fread(&vmdLightCount, sizeof(vmdLightCount), 1, fp);

	vector<VMDLight> lights(vmdLightCount);
	fread(lights.data(), sizeof(VMDLight), vmdLightCount, fp);

#pragma pack(1)
	struct VMDSelfShadow
	{
		uint32_t frameNo;
		uint8_t mode;
		float distance;
	};
#pragma pack()
	uint32_t selfShadowCount = 0;
	fread(&selfShadowCount, sizeof(selfShadowCount), 1, fp);
	vector<VMDSelfShadow> selfShadowData(selfShadowCount);
	fread(selfShadowData.data(), sizeof(VMDSelfShadow), selfShadowCount, fp);

	uint32_t ikSwitchCount = 0;
	fread(&ikSwitchCount, sizeof(ikSwitchCount), 1, fp);

	_ikEnableData.resize(ikSwitchCount);
	for (auto& ikEnable : _ikEnableData)
	{
		fread(&ikEnable.frameNo, sizeof(ikEnable.frameNo), 1, fp);

		uint8_t visibleFlg = 0;
		fread(&visibleFlg, sizeof(visibleFlg), 1, fp);

		uint32_t ikBoneCount = 0;
		fread(&ikBoneCount, sizeof(ikBoneCount), 1, fp);

		for (int i = 0; i < ikBoneCount; ++i)
		{
			char ikBoneName[20];
			fread(ikBoneName, _countof(ikBoneName), 1, fp);
			uint8_t flg = 0;
			fread(&flg, sizeof(flg), 1, fp);
			ikEnable.ikEnableTable[ikBoneName] = flg;
		}
	}

	fclose(fp);
}

HRESULT PMDActor::CreateTransformView(Dx12Wrapper& dx)
{
	auto buffSize = sizeof(XMMATRIX) * (1 + _boneMatrices.size());
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

	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);

	return S_OK;
}

HRESULT PMDActor::CreateMaterialData(Dx12Wrapper& dx)
{
	int materialBufferSize = sizeof(MaterialForHlsl);
	materialBufferSize = (materialBufferSize + 0xff) & ~0xff;

	auto result = dx.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize * _materials.size()),
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

	for (auto& m : _materials)
	{
		*((MaterialForHlsl*)mapMaterial) = m.material;
		mapMaterial += materialBufferSize;
	}

	_materialBuff->Unmap(0, nullptr);

	return S_OK;
}

HRESULT PMDActor::CreateMaterialAndTextureView(Dx12Wrapper& dx)
{
	D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
	matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	matDescHeapDesc.NodeMask = 0;
	matDescHeapDesc.NumDescriptors = _materials.size() * 5;
	matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	auto result = dx.Device()->CreateDescriptorHeap(&matDescHeapDesc, IID_PPV_ARGS(_materialHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	auto materialBuffSize = sizeof(MaterialForHlsl);
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

	for (int i = 0; i < _materials.size(); ++i)
	{
		dx.Device()->CreateConstantBufferView(&matCBVDesc, matDescHeapH);

		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;

		if (_textureResources[i] == nullptr)
		{
			srvDesc.Format = dx.GetWhiteTexture()->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(dx.GetWhiteTexture().Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _textureResources[i]->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(
				_textureResources[i].Get(),
				&srvDesc,
				matDescHeapH);
		}

		matDescHeapH.ptr += incSize;

		if (_sphResources[i] == nullptr)
		{
			srvDesc.Format = dx.GetWhiteTexture()->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(dx.GetWhiteTexture().Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _sphResources[i]->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(
				_sphResources[i].Get(),
				&srvDesc,
				matDescHeapH);
		}

		matDescHeapH.ptr += incSize;

		if (_spaResources[i] == nullptr)
		{
			srvDesc.Format = dx.GetBlackTexture()->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(dx.GetBlackTexture().Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _spaResources[i]->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(
				_spaResources[i].Get(),
				&srvDesc,
				matDescHeapH);
		}

		matDescHeapH.ptr += incSize;

		if (_toonResources[i] == nullptr)
		{
			srvDesc.Format = dx.GetGradTexture()->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(dx.GetGradTexture().Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _toonResources[i]->GetDesc().Format;
			dx.Device()->CreateShaderResourceView(_toonResources[i].Get(), &srvDesc, matDescHeapH);
		}

		matDescHeapH.ptr += incSize;
	}

	return result;
}

void PMDActor::MotionUpdate()
{
	DWORD elapsedTime = timeGetTime() - _startTime;
	unsigned int frameNo = 30 * (elapsedTime / 1000.0f);

	if (frameNo > _duration)
	{
		_startTime = timeGetTime();
		frameNo = 0;
	}

	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	for (auto& bonemotion : _motionData)
	{
		auto itBoneNode = _boneNodeTable.find(bonemotion.first);
		if (itBoneNode == _boneNodeTable.end())
		{
			continue;
		}

		auto& node = itBoneNode->second;

		auto motions = bonemotion.second;
		auto rit = std::find_if(
			motions.rbegin(), motions.rend(),
			[frameNo](const Motion& motion)
			{
				return motion.frameNo <= frameNo;
			}
		);

		XMMATRIX rotation = XMMatrixIdentity();
		XMVECTOR offset = XMLoadFloat3(&rit->offset);

		auto it = rit.base();

		if (it != motions.end())
		{
			auto t = static_cast<float>(frameNo - rit->frameNo) / static_cast<float>(it->frameNo - rit->frameNo);

			t = GetYFromXOnBezier(t, it->p1, it->p2, 12);

			rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(rit->quaternion, it->quaternion, t));
			offset = XMVectorLerp(offset, XMLoadFloat3(&it->offset), t);
		}
		else
		{
			rotation = XMMatrixRotationQuaternion(rit->quaternion);
		}

		auto& pos = node.startPos;

		auto mat =
			XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
			* rotation
			* XMMatrixTranslation(pos.x, pos.y, pos.z);
		_boneMatrices[node.boneIdx] = mat * XMMatrixTranslationFromVector(offset);
	}

	auto centerNode = _boneNodeTableByIdx[0];
	RecursiveMatrixMultiply(&centerNode, XMMatrixIdentity());

	//IKSolve(frameNo);

	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

void PMDActor::IKSolve(int frameNo)
{
	auto it = find_if(_ikEnableData.rbegin(), _ikEnableData.rend(),
		[frameNo](const VMDIKEnable& ikenalbe)
		{
			return ikenalbe.frameNo <= frameNo;
		});

	for (auto& ik : _ikData)
	{
		if (it != _ikEnableData.rend())
		{
			auto ikEnableIt = it->ikEnableTable.find(_boneNameArray[ik.boneIdx]);
			if (ikEnableIt != it->ikEnableTable.end())
			{
				if (!ikEnableIt->second)
				{
					continue;
				}
			}
		}

		auto childrenNodesCount = ik.nodeIdx.size();

		switch (childrenNodesCount)
		{
		case 0:
			assert(0);
			continue;
		case 1:
			SolveLookAt(ik);
			break;
		case 2:
			SolveCosineIK(ik);
			break;
		default:
			SolveCCDIK(ik);
		}
	}
}

void PMDActor::SolveCCDIK(const PMDIK& ik)
{
	auto targetBoneNode = _boneNodeAddressArray[ik.boneIdx];
	auto targetOriginPos = XMLoadFloat3(&targetBoneNode->startPos);

	auto parentMat = _boneMatrices[_boneNodeAddressArray[ik.boneIdx]->ikParentBone];
	XMVECTOR det;
	auto invParentMat = XMMatrixInverse(&det, parentMat);
	auto targetNextPos = XMVector3Transform(targetOriginPos, _boneMatrices[ik.boneIdx] * invParentMat);


	std::vector<XMVECTOR> bonePositions;

	auto endPos = XMLoadFloat3(&_boneNodeAddressArray[ik.targetIdx]->startPos);
	for (auto& cidx : ik.nodeIdx)
	{
		bonePositions.push_back(XMLoadFloat3(&_boneNodeAddressArray[cidx]->startPos));
	}

	vector<XMMATRIX> mats(bonePositions.size());
	fill(mats.begin(), mats.end(), XMMatrixIdentity());

	auto ikLimit = ik.limit * XM_PI;
	for (int c = 0; c < ik.iterations; ++c) {
		if (XMVector3Length(XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon) {
			break;
		}
		for (int bidx = 0; bidx < bonePositions.size(); ++bidx) {
			const auto& pos = bonePositions[bidx];

			auto vecToEnd = XMVectorSubtract(endPos, pos);
			auto vecToTarget = XMVectorSubtract(targetNextPos, pos);
			vecToEnd = XMVector3Normalize(vecToEnd);
			vecToTarget = XMVector3Normalize(vecToTarget);

			if (XMVector3Length(XMVectorSubtract(vecToEnd, vecToTarget)).m128_f32[0] <= epsilon) {
				continue;
			}

			auto cross = XMVector3Normalize(XMVector3Cross(vecToEnd, vecToTarget));
			float angle = XMVector3AngleBetweenVectors(vecToEnd, vecToTarget).m128_f32[0];
			angle = min(angle, ikLimit);
			XMMATRIX rot = XMMatrixRotationAxis(cross, angle);

			auto mat = XMMatrixTranslationFromVector(-pos) *
				rot *
				XMMatrixTranslationFromVector(pos);
			mats[bidx] *= mat;

			for (auto idx = bidx - 1; idx >= 0; --idx) {
				bonePositions[idx] = XMVector3Transform(bonePositions[idx], mat);
			}
			endPos = XMVector3Transform(endPos, mat);
			if (XMVector3Length(XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon) {
				break;
			}
		}
	}
	int idx = 0;
	for (auto& cidx : ik.nodeIdx) {
		_boneMatrices[cidx] = mats[idx];
		++idx;
	}
	auto node = _boneNodeAddressArray[ik.nodeIdx.back()];
	RecursiveMatrixMultiply(node, parentMat);

}

void PMDActor::SolveCosineIK(const PMDIK& ik)
{
	vector<XMVECTOR> positions;//IK?맟?귩뺎뫔
	std::array<float, 2> edgeLens;//IK궻궩귢궪귢궻??깛듩궻떁뿣귩뺎뫔

	//??긒긞긣(뼎???깛궳궼궶궘갂뼎???깛궕뗟궱궘뽞뷭??깛궻띆뷭귩롦벦)
	auto& targetNode = _boneNodeAddressArray[ik.boneIdx];
	auto targetPos = XMVector3Transform(XMLoadFloat3(&targetNode->startPos), _boneMatrices[ik.boneIdx]);

	//IK?긃?깛궕땤룈궶궻궳갂땤궸빥귆귝궎궸궢궲궋귡
	//뼎???깛
	auto endNode = _boneNodeAddressArray[ik.targetIdx];
	positions.emplace_back(XMLoadFloat3(&endNode->startPos));
	//뭷듩땩귂깑?긣??깛
	for (auto& chainBoneIdx : ik.nodeIdx) {
		auto boneNode = _boneNodeAddressArray[chainBoneIdx];
		positions.emplace_back(XMLoadFloat3(&boneNode->startPos));
	}
	//궭귛궯궴빁궔귟궱귞궋궴럙궯궫궻궳땤궸궢궲궓궖귏궥갃궩궎궳귖궶궋릐궼궩궻귏귏
	//똶럁궢궲귖귞궯궲?귦궶궋궳궥갃
	reverse(positions.begin(), positions.end());

	//뙰궻뮮궠귩뫇궯궲궓궘
	edgeLens[0] = XMVector3Length(XMVectorSubtract(positions[1], positions[0])).m128_f32[0];
	edgeLens[1] = XMVector3Length(XMVectorSubtract(positions[2], positions[1])).m128_f32[0];

	//깑?긣??깛띆뷭빾듂(땤룈궸궶궯궲궋귡궫귕럊뾭궥귡귽깛긢긞긏긚궸뭾댰)
	positions[0] = XMVector3Transform(positions[0], _boneMatrices[ik.nodeIdx[1]]);
	//?귪뭷궼궵궎궧렔벍똶럁궠귢귡궻궳똶럁궢궶궋
	//먩???깛
	positions[2] = XMVector3Transform(positions[2], _boneMatrices[ik.boneIdx]);//긼깛?궼ik.targetIdx궬궕갷갏갎

	//깑?긣궔귞먩?귉궻긹긏긣깑귩띿궯궲궓궘
	auto linearVec = XMVectorSubtract(positions[2], positions[0]);
	float A = XMVector3Length(linearVec).m128_f32[0];
	float B = edgeLens[0];
	float C = edgeLens[1];

	linearVec = XMVector3Normalize(linearVec);

	//깑?긣궔귞?귪뭷귉궻둷뱗똶럁
	float theta1 = acosf((A * A + B * B - C * C) / (2 * A * B));

	//?귪뭷궔귞??긒긞긣귉궻둷뱗똶럁
	float theta2 = acosf((B * B + C * C - A * A) / (2 * B * C));

	//걏렡걐귩땫귕귡
	//귖궢?귪뭷궕걏궿궡걐궳궇궯궫뤾뜃궸궼떗맕밒궸X렡궴궥귡갃
	XMVECTOR axis;
	if (find(_kneeIdxes.begin(), _kneeIdxes.end(), ik.nodeIdx[0]) == _kneeIdxes.end()) {
		auto vm = XMVector3Normalize(XMVectorSubtract(positions[2], positions[0]));
		auto vt = XMVector3Normalize(XMVectorSubtract(targetPos, positions[0]));
		axis = XMVector3Cross(vt, vm);
	}
	else
	{
		auto right = XMFLOAT3(1, 0, 0);
		axis = XMLoadFloat3(&right);
	}

	//if (XMVector3Equal(axis, XMVectorZero()))
	//{
	//	auto right = XMFLOAT3(1, 0, 0);
	//	axis = XMLoadFloat3(&right);
	//}

	//뭾댰?갷IK?긃?깛궼뜧궯궞궸뛀궔궯궲궔귞릶궑귞귢귡궫귕1궕뜧궯궞궸뗟궋
	auto mat1 = XMMatrixTranslationFromVector(-positions[0]);
	mat1 *= XMMatrixRotationAxis(axis, theta1);
	mat1 *= XMMatrixTranslationFromVector(positions[0]);


	auto mat2 = XMMatrixTranslationFromVector(-positions[1]);
	mat2 *= XMMatrixRotationAxis(axis, theta2 - XM_PI);
	mat2 *= XMMatrixTranslationFromVector(positions[1]);

	_boneMatrices[ik.nodeIdx[1]] *= mat1;
	_boneMatrices[ik.nodeIdx[0]] = mat2 * _boneMatrices[ik.nodeIdx[1]];
	_boneMatrices[ik.targetIdx] = _boneMatrices[ik.nodeIdx[0]];//뮳멟궻뎓떯귩롷궚귡
	//_boneMatrices[ik.nodeIdxes[1]] = _boneMatrices[ik.boneIdx];
	//_boneMatrices[ik.nodeIdxes[0]] = _boneMatrices[ik.boneIdx];
	//_boneMatrices[ik.targetIdx] *= _boneMatrices[ik.boneIdx];
}

void PMDActor::SolveLookAt(const PMDIK& ik)
{
	auto rootNode = _boneNodeAddressArray[ik.nodeIdx[0]];
	auto targetNode = _boneNodeAddressArray[ik.targetIdx];

	auto rpos1 = XMLoadFloat3(&rootNode->startPos);
	auto tpos1 = XMLoadFloat3(&targetNode->startPos);

	auto rpos2 = XMVector3TransformCoord(rpos1, _boneMatrices[ik.nodeIdx[0]]);
	auto tpos2 = XMVector3TransformCoord(tpos1, _boneMatrices[ik.boneIdx]);

	auto originVec = XMVectorSubtract(tpos1, rpos1);
	auto targetVec = XMVectorSubtract(tpos2, rpos2);

	originVec = XMVector3Normalize(originVec);
	targetVec = XMVector3Normalize(targetVec);

	XMMATRIX mat = XMMatrixTranslationFromVector(-rpos2) *
		LookAtMatrix(originVec, targetVec, XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0)) *
		XMMatrixTranslationFromVector(rpos2);

	_boneMatrices[ik.nodeIdx[0]] = mat;
}

void PMDActor::RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat)
{
	_boneMatrices[node->boneIdx] *= mat;

	for (auto& cnode : node->children)
	{
		RecursiveMatrixMultiply(cnode, _boneMatrices[node->boneIdx]);
	}
}

float PMDActor::GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n)
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

