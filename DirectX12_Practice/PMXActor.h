#pragma once
#include<d3d12.h>
#include<DirectXMath.h>
#include<vector>
#include<array>
#include<string>
#include<wrl.h>
#include<algorithm>
#include<unordered_map>
#include<thread>
#include<future>

#include "PmxFileData.h"
#include "VMDFileData.h"

using namespace DirectX;

struct VMDKey
{
	unsigned int frameNo;
	XMVECTOR quaternion;
	XMFLOAT3 offset;
	XMFLOAT2 p1;
	XMFLOAT2 p2;

	VMDKey(unsigned int frameNo, XMVECTOR& quaternion, XMFLOAT3& offset, XMFLOAT2& p1, XMFLOAT2& p2):
	frameNo(frameNo),
	quaternion(quaternion),
	offset(offset),
	p1(p1),
	p2(p2)
	{}
};

struct BoneNode
{
	unsigned int boneIndex;

	std::wstring name;
	std::string englishName;

	DirectX::XMFLOAT3 position;
	unsigned int parentBoneIndex;
	unsigned int deformDepth;

	PMXBoneFlags boneFlag;

	DirectX::XMFLOAT3 positionOffset;
	unsigned int linkBoneIndex;

	unsigned int appendBoneIndex;
	float appendWeight;

	DirectX::XMFLOAT3 fixedAxis;
	DirectX::XMFLOAT3 localXAxis;
	DirectX::XMFLOAT3 localZAxis;

	unsigned int keyValue;

	unsigned int ikTargetBoneIndex;
	unsigned int ikIterationCount;
	unsigned int ikLimit;

	BoneNode* parentNode;
	std::vector<BoneNode*> childrenNode;
};

struct UploadVertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 uv;
};

struct SkinningRange
{
	unsigned int startIndex;
	unsigned int vertexCount;
};

class Dx12Wrapper;
class PMXActor
{
public:
	PMXActor();
	~PMXActor();

	bool Initialize(const std::wstring& filePath, Dx12Wrapper& dx);
	void Update();
	void UpdateAnimation();
	void Draw(Dx12Wrapper& dx, bool isShadow);

private:
	HRESULT CreateVbAndIb(Dx12Wrapper& dx);
	HRESULT CreateTransformView(Dx12Wrapper& dx);
	HRESULT CreateMaterialData(Dx12Wrapper& dx);
	HRESULT CreateMaterialAndTextureView(Dx12Wrapper& dx);

	void LoadVertexData(const std::vector<PMXVertex>& vertices);

	void InitAnimation(VMDFileData& vmdFileData);
	void InitBoneNode(std::vector<PMXBone>& bones);
	float GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n);
	void RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat);

	void InitParallelVertexSkinningSetting();

	void VertexSkinning();
	void VertexSkinningByRange(const SkinningRange& range);

private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	PMXFileData _pmxFileData;
	VMDFileData _vmdFileData;

	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	UploadVertex* _mappedVertex;
	std::vector<UploadVertex> _uploadVertices;

	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;
	std::vector<ComPtr<ID3D12Resource>> _sphereTextureResources;

	ComPtr<ID3D12Resource> _transformMat = nullptr;
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;
	std::vector<XMMATRIX> _boneMatrices;
	std::vector<XMMATRIX> _boneLocalMatrices;

	struct Transform
	{
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	Transform _transform;
	DirectX::XMMATRIX* _mappedMatrices;
	DirectX::XMMATRIX* _mappedLocalMatrices;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	ComPtr<ID3D12DescriptorHeap> _materialHeap = nullptr;

	struct MaterialForShader
	{
		XMFLOAT4 diffuse;
		XMFLOAT3 specular;
		float specularPower;
		XMFLOAT3 ambient;
	};

	std::unordered_map<std::wstring, std::vector<VMDKey>> _animationKeyMap;
	unsigned int _duration;

	std::vector<std::wstring> _boneNodeNames;
	std::vector<BoneNode*> _boneNodeByIdx;
	std::unordered_map<std::wstring, BoneNode> _boneNodeByName;

	DWORD _startTime = 0;

	std::vector<SkinningRange> _skinningRanges;
	std::vector<std::future<void>> _parallelUpdateFutures;
};

