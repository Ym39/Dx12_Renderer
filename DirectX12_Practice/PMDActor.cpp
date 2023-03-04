#include "PMDActor.h"
#include "PMDRenderer.h"
#include "Dx12Wrapper.h"
#include <d3dx12.h>

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

#pragma comment(lib,"winmm.lib")

namespace
{
	std::string GetExtension(const std::string& path)
	{
		int idx = path.rfind(".");
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	std::pair<std::string, std::string> SplitFileName(const std::string path, const char splitter = '*')
	{
		int idx = path.find(splitter);
		pair<std::string, std::string> ret;
		ret.first = path.substr(0, idx);
		ret.second = path.substr(idx + 1, path.length() - idx - 1);
		return ret;
	}

	std::string GetTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath)
	{
		auto pathIndex1 = modelPath.rfind('/');
		auto folderPath = modelPath.substr(0, pathIndex1);

		//string texPathString = texPath;

		//size_t nPos = texPathString.find(".bmp");

		//if (nPos != string::npos)
		//{
		//	texPathString = texPathString.substr(0, nPos + 4);
		//}

		return folderPath + "/" + texPath;
	}
}

void* PMDActor::Transform::operator new(size_t size) {
	return _aligned_malloc(size, 16);
}

HRESULT PMDActor::CreateMaterialData()
{
	int materialBufferSize = sizeof(MaterialForHlsl);
	materialBufferSize = (materialBufferSize + 0xff) & ~0xff;

	auto result = _dx12.Device()->CreateCommittedResource(
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

HRESULT PMDActor::CreateMaterialAndTextureView()
{
	D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
	matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	matDescHeapDesc.NodeMask = 0;
	matDescHeapDesc.NumDescriptors = _materials.size() * 5;
	matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	auto result = _dx12.Device()->CreateDescriptorHeap(&matDescHeapDesc, IID_PPV_ARGS(_materialHeap.ReleaseAndGetAddressOf()));
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
	auto incSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (int i = 0; i < _materials.size(); ++i)
	{
		_dx12.Device()->CreateConstantBufferView(&matCBVDesc, matDescHeapH);

		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;

		if (_textureResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._whiteTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_renderer._whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _textureResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(
				_textureResources[i].Get(),
				&srvDesc,
				matDescHeapH);
		}

		matDescHeapH.ptr += incSize;

		if (_sphResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._whiteTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_renderer._whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _sphResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(
				_sphResources[i].Get(),
				&srvDesc,
				matDescHeapH);
		}

		matDescHeapH.ptr += incSize;

		if (_spaResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._blackTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_renderer._blackTex.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _spaResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(
				_spaResources[i].Get(),
				&srvDesc,
				matDescHeapH);
		}

		matDescHeapH.ptr += incSize;

		if (_toonResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._gradTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_renderer._gradTex.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _toonResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_toonResources[i].Get(), &srvDesc, matDescHeapH);
		}

		matDescHeapH.ptr += incSize;
	}

	return result;
}

HRESULT PMDActor::CreateTransformView()
{
	auto buffSize = sizeof(XMMATRIX) * (1 + _boneMatrices.size());
	buffSize = (buffSize + 0xff) & ~0xff;

	auto result = _dx12.Device()->CreateCommittedResource(
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

	result = _dx12.Device()->CreateDescriptorHeap(&transformDescHeapDesc, IID_PPV_ARGS(_transformHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _transformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;

	_dx12.Device()->CreateConstantBufferView(&cbvDesc, _transformHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

HRESULT PMDActor::LoadPMDFile(const char* path)
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

	unsigned int vertNum;
	fread(&vertNum, sizeof(vertNum), 1, fp);

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
	std::vector<unsigned char> vertices(vertNum* pmdvertex_size);
	fread(vertices.data(), vertices.size(), 1, fp);

	unsigned int indicesNum;
	fread(&indicesNum, sizeof(indicesNum), 1, fp);

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

	auto result = _dx12.Device()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(_vb.ReleaseAndGetAddressOf()));

	unsigned char* vertMap = nullptr;

	result = _vb->Map(0, nullptr, (void**)&vertMap);

	std::copy(std::begin(vertices), std::end(vertices), vertMap);

	_vb->Unmap(0, nullptr);

	_vbView.BufferLocation = _vb->GetGPUVirtualAddress();
	_vbView.SizeInBytes = vertices.size();
	_vbView.StrideInBytes = pmdvertex_size;

	std::vector<unsigned short> indices;

	indices.resize(indicesNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

	resdesc.Width = indices.size() * sizeof(indices[0]);

	result = _dx12.Device()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(_ib.ReleaseAndGetAddressOf()));

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

		_toonResources[i] = _dx12.GetTextureByPath(toonFilePath.c_str());

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
			_textureResources[i] = _dx12.GetTextureByPath(texFilePath.c_str());
		}
		if (sphFileName != "")
		{
			auto sphFilePath = GetTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
			_sphResources[i] = _dx12.GetTextureByPath(sphFilePath.c_str());
		}
		if (spaFileName != "")
		{
			auto spaFilePath = GetTexturePathFromModelAndTexPath(strModelPath, spaFileName.c_str());
			_spaResources[i] = _dx12.GetTextureByPath(spaFilePath.c_str());
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

	//º»
	unsigned short boneNum = 0;
	fread(&boneNum, sizeof(boneNum), 1, fp);

	std::vector<PMDBone> pmdBones(boneNum);
	fread(pmdBones.data(), sizeof(PMDBone), boneNum, fp);

	std::vector<std::string> boneNames(pmdBones.size());

	for (int idx = 0; idx < pmdBones.size(); ++idx)
	{
		auto& pd = pmdBones[idx];
		boneNames[idx] = pd.boneName;
		auto& node = _boneNodeTable[pd.boneName];
		node.boneIdx = idx;
		node.startPos = pd.pos;
	}

	for (auto& pb : pmdBones)
	{
		if (pb.parentNo >= pmdBones.size())
		{
			continue;
		}

		auto parentName = boneNames[pb.parentNo];
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

	for (auto& bonemotion : _motiondata)
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

		XMMATRIX rotation;
		auto it = rit.base();

		if (it != motions.end())
		{
			auto t = static_cast<float>(frameNo - rit->frameNo) / static_cast<float>(it->frameNo - rit->frameNo);

			t = GetYFromXOnBezier(t, it->p1, it->p2, 12);

			rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(rit->quaternion, it->quaternion, t));
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
		_boneMatrices[node.boneIdx] = mat;
	}

	auto centerNode = _boneNodeTableByIdx[0];
	RecursiveMatrixMultiply(&centerNode, XMMatrixIdentity());
	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
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

	constexpr float epsilon = 0.0005f;

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

PMDActor::PMDActor(const char* filepath, PMDRenderer& renderer):
	_renderer(renderer),
	_dx12(renderer._dx12),
	_angle(0.0f)
{
	_transform.world = XMMatrixIdentity() * XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	LoadPMDFile(filepath);
	LoadVMDFile("Model/Nyan cat EX.vmd");
	CreateTransformView();
	CreateMaterialData();
	CreateMaterialAndTextureView();
}

PMDActor::~PMDActor()
{
}

void PMDActor::LoadVMDFile(const char* filepath)
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

	for (auto& vmdMotion : vmdMotionData)
	{
		_motiondata[vmdMotion.boneName].
			emplace_back(Motion(vmdMotion.frameNo, 
				XMLoadFloat4(&vmdMotion.quaternion),
				XMFLOAT2((float)vmdMotion.bezier[3]/127.0f, (float)vmdMotion.bezier[7]/127.0f),
				XMFLOAT2((float)vmdMotion.bezier[11]/127.0f, (float)vmdMotion.bezier[15]/127.0f)));
	}

	for (auto& motions : _motiondata)
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

void PMDActor::PlayAnimation()
{
	_startTime = timeGetTime();
}

void PMDActor::Update()
{
	_angle += 0.005f;
	//_mappedMatrices[0] = XMMatrixRotationY(_angle) * XMMatrixTranslation(0.0f, 0.0f, 0.0f);

	MotionUpdate();
}

void PMDActor::Draw()
{
	_dx12.CommandList()->IASetVertexBuffers(0, 1, &_vbView);
	_dx12.CommandList()->IASetIndexBuffer(&_ibView);

	ID3D12DescriptorHeap* transheap[] = { _transformHeap.Get() };
	_dx12.CommandList()->SetDescriptorHeaps(1, transheap);
	_dx12.CommandList()->SetGraphicsRootDescriptorTable(1, _transformHeap->GetGPUDescriptorHandleForHeapStart());

	ID3D12DescriptorHeap* mdh[] = { _materialHeap.Get() };

	_dx12.CommandList()->SetDescriptorHeaps(1, mdh);

	auto materialH = _materialHeap->GetGPUDescriptorHandleForHeapStart();
	unsigned int idxOffset = 0;

	auto cbvsrvIncSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;
	for (auto& m : _materials)
	{
		_dx12.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
		_dx12.CommandList()->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}
}
