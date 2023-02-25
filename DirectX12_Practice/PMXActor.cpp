#include "PMXActor.h"
#include <fstream>
#include <array>
#include <bitset>
#include "Dx12Wrapper.h"
#include "PMXRenderer.h"
#include "srtconv.h"
#include <d3dx12.h>

using namespace std;

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

	char* ConvertWcToC(const wchar_t* str)
	{
		char* pStr;
		int strSize = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
		pStr = new char[strSize];
		WideCharToMultiByte(CP_ACP, 0, str, -1, pStr, strSize, 0, 0);

		return pStr;
	}

	string WStringToString(const wstring& oWString)
	{
		// wstring → SJIS
		int iBufferSize = WideCharToMultiByte(CP_OEMCP, 0, oWString.c_str()
			, -1, (char*)NULL, 0, NULL, NULL);

		// バッファの取得
		CHAR* cpMultiByte = new CHAR[iBufferSize];

		// wstring → SJIS
		WideCharToMultiByte(CP_OEMCP, 0, oWString.c_str(), -1, cpMultiByte
			, iBufferSize, NULL, NULL);

		// stringの生成
		std::string oRet(cpMultiByte, cpMultiByte + iBufferSize - 1);

		// バッファの破棄
		delete[] cpMultiByte;

		// 変換結果を返す
		return(oRet);
	}
}

bool getPMXStringUTF16(std::ifstream& _file, std::wstring& output)
{
	std::array<wchar_t, 512> wBuffer{};
	int textSize;

	_file.read(reinterpret_cast<char*>(&textSize), 4);

	_file.read(reinterpret_cast<char*>(&wBuffer), textSize);
	output = std::wstring(&wBuffer[0], &wBuffer[0] + textSize / 2);

	return true;
}

bool PMXActor::loadPMX(PMXModelData& data, const std::wstring& _filePath)
{
	if (_filePath.empty()) { return false; }

	//모델파일 패스에서 모델폴더 패스 추출
	std::wstring folderPath{ _filePath.begin(), _filePath.begin() + _filePath.rfind(L'\\') + 1 };

	// 파일 오픈
	std::ifstream pmxFile{ _filePath, (std::ios::binary | std::ios::in) };
	if (pmxFile.fail())
	{
		pmxFile.close();
		return false;
	}

	// 헤더
	std::array<unsigned char, 4> pmxHeader{};
	constexpr std::array<unsigned char, 4> PMX_MAGIC_NUMBER{ 0x50, 0x4d, 0x58, 0x20 };
	enum HeaderDataIndex
	{
		ENCODING_FORMAT,
		NUMBER_OF_ADD_UV,
		VERTEX_INDEX_SIZE,
		TEXTURE_INDEX_SIZE,
		MATERIAL_INDEX_SIZE,
		BONE_INDEX_SIZE,
		RIGID_BODY_INDEX_SIZE
	};

	for (int i = 0; i < 4; i++)
	{
		pmxHeader[i] = pmxFile.get();
	}
	if (pmxHeader != PMX_MAGIC_NUMBER)
	{
		pmxFile.close();
		return false;
	}

	// ver2.0이외는 비대응
	float version{};
	pmxFile.read(reinterpret_cast<char*>(&version), 4);
	if (!XMScalarNearEqual(version, 2.0f, g_XMEpsilon.f[0]))
	{
		pmxFile.close();
		return false;
	}

	unsigned char hederDataLength = pmxFile.get();
	if (hederDataLength != 8)
	{
		pmxFile.close();
		return false;
	}
	std::array<unsigned char, 8> hederData{};
	for (int i = 0; i < hederDataLength; i++)
	{
		hederData[i] = pmxFile.get();
	}
	//UTF-8은 비대응
	if (hederData[0] != 0)
	{
		pmxFile.close();
		return false;
	}

	unsigned arrayLength{};
	for (int i = 0; i < 4; i++)
	{
		pmxFile.read(reinterpret_cast<char*>(&arrayLength), 4);
		for (unsigned j = 0; j < arrayLength; j++)
		{
			pmxFile.get();
		}
	}

	// 버텍스 -----------------------------------
	using Vertex = PMXModelData::Vertex;
	int numberOfVertex{};
	pmxFile.read(reinterpret_cast<char*>(&numberOfVertex), 4);
	data.vertices.resize(numberOfVertex);
	data.verticesForShader.resize(numberOfVertex);

	for (int i = 0; i < numberOfVertex; i++)
	{
		pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].position), 12);
		pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].normal), 12);
		pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].uv), 8);
		if (hederData[NUMBER_OF_ADD_UV] != 0)
		{
			for (int j = 0; j < hederData[NUMBER_OF_ADD_UV]; ++j)
			{
				pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].additionalUV[j]), 16);
			}
		}

		const unsigned char weightMethod = pmxFile.get();
		switch (weightMethod)
		{
		case Vertex::Weight::BDEF1:
			data.vertices[i].weight.type = Vertex::Weight::BDEF1;
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.born1), hederData[BONE_INDEX_SIZE]);
			data.vertices[i].weight.born2 = PMXModelData::NO_DATA_FLAG;
			data.vertices[i].weight.born3 = PMXModelData::NO_DATA_FLAG;
			data.vertices[i].weight.born4 = PMXModelData::NO_DATA_FLAG;
			data.vertices[i].weight.weight1 = 1.0f;
			break;

		case Vertex::Weight::BDEF2:
			data.vertices[i].weight.type = Vertex::Weight::BDEF2;
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.born1), hederData[BONE_INDEX_SIZE]);
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.born2), hederData[BONE_INDEX_SIZE]);
			data.vertices[i].weight.born3 = PMXModelData::NO_DATA_FLAG;
			data.vertices[i].weight.born4 = PMXModelData::NO_DATA_FLAG;
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.weight1), 4);
			data.vertices[i].weight.weight2 = 1.0f - data.vertices[i].weight.weight1;
			break;

		case Vertex::Weight::BDEF4:
			data.vertices[i].weight.type = Vertex::Weight::BDEF4;
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.born1), hederData[BONE_INDEX_SIZE]);
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.born2), hederData[BONE_INDEX_SIZE]);
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.born3), hederData[BONE_INDEX_SIZE]);
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.born4), hederData[BONE_INDEX_SIZE]);
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.weight1), 4);
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.weight2), 4);
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.weight3), 4);
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.weight4), 4);
			break;

		case Vertex::Weight::SDEF:
			data.vertices[i].weight.type = Vertex::Weight::SDEF;
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.born1), hederData[BONE_INDEX_SIZE]);
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.born2), hederData[BONE_INDEX_SIZE]);
			data.vertices[i].weight.born3 = PMXModelData::NO_DATA_FLAG;
			data.vertices[i].weight.born4 = PMXModelData::NO_DATA_FLAG;
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.weight1), 4);
			data.vertices[i].weight.weight2 = 1.0f - data.vertices[i].weight.weight1;
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.c), 12);
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.r0), 12);
			pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].weight.r1), 12);
			break;

		default:
			pmxFile.close();
			return false;
		}

		pmxFile.read(reinterpret_cast<char*>(&data.vertices[i].edgeMagnif), 4);

		if (data.vertices[i].weight.born1 == PMXModelData::NO_DATA_FLAG)
		{
			pmxFile.close();
			return false;
		}

		data.verticesForShader[i].position = data.vertices[i].position;
		data.verticesForShader[i].normal = data.vertices[i].normal;
		data.verticesForShader[i].uv = data.vertices[i].uv;
	}

	// 폴리곤  ------------------------------------
	int numOfSurface{};
	pmxFile.read(reinterpret_cast<char*>(&numOfSurface), 4);
	data.indices.resize(numOfSurface);

	for (int i = 0; i < numOfSurface; i++)
	{
		pmxFile.read(reinterpret_cast<char*>(&data.indices[i]), hederData[VERTEX_INDEX_SIZE]);

		if (data.indices[i] == PMXModelData::NO_DATA_FLAG || data.indices[i] == PMXModelData::NO_DATA_FLAG || data.indices[i] == PMXModelData::NO_DATA_FLAG)
		{
			pmxFile.close();
			return false;
		}
	}

	// 텍스쳐 -----------------------------
	int numOfTexture{};
	pmxFile.read(reinterpret_cast<char*>(&numOfTexture), 4);
	data.texturePaths.resize(numOfTexture);

	std::wstring texturePath{};
	for (int i = 0; i < numOfTexture; i++)
	{
		data.texturePaths[i] = folderPath;
		getPMXStringUTF16(pmxFile, texturePath);
		data.texturePaths[i] += texturePath;
	}

	// 메터리얼 -----------------------------
	int numOfMaterial{};
	pmxFile.read(reinterpret_cast<char*>(&numOfMaterial), 4);

	data.materials.resize(numOfMaterial);
	for (int i = 0; i < numOfMaterial; i++)
	{
		for (int j = 0; j < 2; ++j)
		{
			pmxFile.read(reinterpret_cast<char*>(&arrayLength), 4);
			for (unsigned i = 0; i < arrayLength; i++)
			{
				pmxFile.get();
			}
		}

		pmxFile.read(reinterpret_cast<char*>(&data.materials[i].material.diffuse), 16);
		pmxFile.read(reinterpret_cast<char*>(&data.materials[i].material.specular), 12);
		pmxFile.read(reinterpret_cast<char*>(&data.materials[i].material.specularity), 4);
		pmxFile.read(reinterpret_cast<char*>(&data.materials[i].material.ambient), 12);

		pmxFile.get();
		for (int i = 0; i < 16; i++)
		{
			pmxFile.get();
		}
		for (int i = 0; i < 4; i++)
		{
			pmxFile.get();
		}

		pmxFile.read(reinterpret_cast<char*>(&data.materials[i].colorMapTextureIndex), hederData[TEXTURE_INDEX_SIZE]);
		for (unsigned char i = 0; i < hederData[TEXTURE_INDEX_SIZE]; i++)
		{
			pmxFile.get();
		}
		pmxFile.get();

		const unsigned char shareToonFlag = pmxFile.get();
		if (shareToonFlag)
		{
			pmxFile.get();
		}
		else
		{
			pmxFile.read(reinterpret_cast<char*>(&data.materials[i].toonTextureIndex), hederData[TEXTURE_INDEX_SIZE]);
		}

		pmxFile.read(reinterpret_cast<char*>(&arrayLength), 4);
		for (unsigned i = 0; i < arrayLength; i++)
		{
			pmxFile.get();
		}

		pmxFile.read(reinterpret_cast<char*>(&data.materials[i].vertexNum), 4);
	}

	// 본 ---------------------------------
	int numOfBone{};
	pmxFile.read(reinterpret_cast<char*>(&numOfBone), 4);

	data.bones.resize(numOfBone);
	int ikLinkSize = 0;
	unsigned char angleLim = 0;

	for (int i = 0; i < numOfBone; i++)
	{
		getPMXStringUTF16(pmxFile, data.bones[i].name);
		pmxFile.read(reinterpret_cast<char*>(&arrayLength), 4);
		data.bones[i].nameEnglish.resize(arrayLength);
		for (unsigned j = 0; j < arrayLength; ++j)
		{
			data.bones[i].nameEnglish[j] = pmxFile.get();
		}

		pmxFile.read(reinterpret_cast<char*>(&data.bones[i].position), 12);

		pmxFile.read(reinterpret_cast<char*>(&data.bones[i].parentIndex), hederData[BONE_INDEX_SIZE]);
		if (numOfBone <= data.bones[i].parentIndex)
		{
			data.bones[i].parentIndex = PMXModelData::NO_DATA_FLAG;
		}

		pmxFile.read(reinterpret_cast<char*>(&data.bones[i].transformationLevel), 4);

		pmxFile.read(reinterpret_cast<char*>(&data.bones[i].flag), 2);

		enum BoneFlagMask
		{
			ACCESS_POINT = 0x0001,
			IK = 0x0020,
			IMPART_TRANSLATION = 0x0100,
			IMPART_ROTATION = 0x0200,
			AXIS_FIXING = 0x0400,
			LOCAL_AXIS = 0x0800,
			EXTERNAL_PARENT_TRANS = 0x2000,
		};

		if (data.bones[i].flag & ACCESS_POINT)
		{
			pmxFile.read(reinterpret_cast<char*>(&data.bones[i].childrenIndex), hederData[BONE_INDEX_SIZE]);
			if (numOfBone <= data.bones[i].childrenIndex)
			{
				data.bones[i].childrenIndex = PMXModelData::NO_DATA_FLAG;
			}
		}
		else
		{
			data.bones[i].childrenIndex = PMXModelData::NO_DATA_FLAG;
			pmxFile.read(reinterpret_cast<char*>(&data.bones[i].coordOffset), 12);
		}
		if ((data.bones[i].flag & IMPART_TRANSLATION) || (data.bones[i].flag & IMPART_ROTATION))
		{
			pmxFile.read(reinterpret_cast<char*>(&data.bones[i].impartParentIndex), hederData[BONE_INDEX_SIZE]);
			pmxFile.read(reinterpret_cast<char*>(&data.bones[i].impartRate), 4);
		}
		if (data.bones[i].flag & AXIS_FIXING)
		{
			pmxFile.read(reinterpret_cast<char*>(&data.bones[i].fixedAxis), 12);
		}
		if (data.bones[i].flag & LOCAL_AXIS)
		{
			pmxFile.read(reinterpret_cast<char*>(&data.bones[i].localAxisX), 12);
			pmxFile.read(reinterpret_cast<char*>(&data.bones[i].localAxisZ), 12);
		}
		if (data.bones[i].flag & EXTERNAL_PARENT_TRANS)
		{
			pmxFile.read(reinterpret_cast<char*>(&data.bones[i].externalParentKey), 4);
		}
		if (data.bones[i].flag & IK)
		{
			pmxFile.read(reinterpret_cast<char*>(&data.bones[i].ikTargetIndex), hederData[5]);
			pmxFile.read(reinterpret_cast<char*>(&data.bones[i].ikLoopCount), 4);
			pmxFile.read(reinterpret_cast<char*>(&data.bones[i].ikUnitAngle), 4);
			pmxFile.read(reinterpret_cast<char*>(&ikLinkSize), 4);
			data.bones[i].ikLinks.resize(ikLinkSize);
			for (int j = 0; j < ikLinkSize; ++j)
			{
				pmxFile.read(reinterpret_cast<char*>(&data.bones[i].ikLinks[j].index), hederData[5]);
				angleLim = pmxFile.get();
				data.bones[i].ikLinks[j].existAngleLimited = false;
				if (angleLim == 1)
				{
					pmxFile.read(reinterpret_cast<char*>(&data.bones[i].ikLinks[j].limitAngleMin), 12);
					pmxFile.read(reinterpret_cast<char*>(&data.bones[i].ikLinks[j].limitAngleMax), 12);
					data.bones[i].ikLinks[j].existAngleLimited = true;
				}
			}
		}
		else
		{
			data.bones[i].ikTargetIndex = PMXModelData::NO_DATA_FLAG;
		}
	}

	pmxFile.close();

	return true;
}

HRESULT PMXActor::CreateVbAndIb()
{

	D3D12_HEAP_PROPERTIES heapprop = {};

	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};

	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = _modelData.verticesForShader.size() * sizeof(_modelData.verticesForShader[0]);
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	auto result = _dx12.Device()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(_vb.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	PMXModelData::VertexForShader* vertMap = nullptr;

	result = _vb->Map(0, nullptr, (void**)&vertMap);

	std::copy(std::begin(_modelData.verticesForShader), std::end(_modelData.verticesForShader), vertMap);

	_vb->Unmap(0, nullptr);

	_vbView.BufferLocation = _vb->GetGPUVirtualAddress();
	_vbView.SizeInBytes = sizeof(_modelData.verticesForShader[0]) * _modelData.verticesForShader.size();
	_vbView.StrideInBytes = sizeof(_modelData.verticesForShader[0]);

	resdesc.Width = _modelData.indices.size() * sizeof(_modelData.indices[0]);

	result = _dx12.Device()->CreateCommittedResource(&heapprop, D3D12_HEAP_FLAG_NONE, &resdesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(_ib.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	unsigned short* mappedIdx = nullptr;
	_ib->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(_modelData.indices), std::end(_modelData.indices), mappedIdx);
	_ib->Unmap(0, nullptr);

	_ibView.BufferLocation = _ib->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = _modelData.indices.size() * sizeof(_modelData.indices[0]);

	size_t materialNum = _modelData.materials.size();
	_textureResources.resize(materialNum);
	_toonResources.resize(materialNum);

	for (int i = 0; i < materialNum; ++i)
	{
		int textureIndex = _modelData.materials[i].colorMapTextureIndex;

		if (_modelData.texturePaths.size() - 1 < textureIndex)
		{
			continue;
		}

		wstring texfilename = _modelData.texturePaths[textureIndex];

		if (texfilename.empty() == false)
		{
			_textureResources[i] = _dx12.GetTextureByPath(texfilename);
		}

		int toonIndex = _modelData.materials[i].toonTextureIndex;

		if (toonIndex == 0 || _modelData.texturePaths.size() - 1 < toonIndex)
		{
			continue;
		}

		wstring toontexfilename = _modelData.texturePaths[toonIndex];

		if (toontexfilename.empty() == false)
		{
			_toonResources[i] = _dx12.GetTextureByPath(toontexfilename);
		}
	}

	return S_OK;
}

HRESULT PMXActor::CreateTransformView()
{
	auto buffSize = sizeof(Transform);
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

	result = _transformBuff->Map(0, nullptr, (void**)&_mappedTransform);

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	*_mappedTransform = _transform;


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

HRESULT PMXActor::CreateMaterialData()
{
	int materialBufferSize = sizeof(MaterialForHlsl);
	materialBufferSize = (materialBufferSize + 0xff) & ~0xff;

	auto result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize * _modelData.materials.size()),
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

	for (auto& m : _modelData.materials)
	{
		*((MaterialForHlsl*)mapMaterial) = m.material;
		mapMaterial += materialBufferSize;
	}

	_materialBuff->Unmap(0, nullptr);

	return S_OK;
}

HRESULT PMXActor::CreateMaterialAndTextureView()
{
	D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
	matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	matDescHeapDesc.NodeMask = 0;
	matDescHeapDesc.NumDescriptors = _modelData.materials.size() * 3;
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

	for (int i = 0; i < _modelData.materials.size(); ++i)
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

PMXActor::PMXActor(const std::wstring& _filePath, PMXRenderer& renderer):
	_renderer(renderer),
	_dx12(renderer._dx12)
{
	_transform.world = XMMatrixIdentity() * XMMatrixTranslation(-20.0f, 0.0f, 0.0f);;
	loadPMX(_modelData, _filePath);
	CreateVbAndIb();
	CreateTransformView();
	CreateMaterialData();
	CreateMaterialAndTextureView();
}

PMXActor::~PMXActor()
{
}

void PMXActor::Update()
{
}

void PMXActor::Draw()
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

	auto cbvsrvIncSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 3;
	for (auto& m : _modelData.materials)
	{
		_dx12.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
		_dx12.CommandList()->DrawIndexedInstanced(m.vertexNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.vertexNum;
	}
}
