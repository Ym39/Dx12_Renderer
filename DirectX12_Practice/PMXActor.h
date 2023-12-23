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

#include "IKSolver.h"
#include "VMDFileData.h"
#include "NodeManager.h"

using namespace DirectX;

struct VMDIKkey
{
	unsigned int frameNo;
	bool enable;

	VMDIKkey(unsigned int frameNo, bool enable):
	frameNo(frameNo),
	enable(enable)
	{}
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
	void InitBoneNode(const std::vector<PMXBone>& bones);
	void InitIK(const std::vector<PMXBone>& bones);
	float GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n);
	void RecursiveMatrixMultiply(BoneNodeLegacy* node, const DirectX::XMMATRIX& mat);

	void InitParallelVertexSkinningSetting();

	void UpdateAnimationIK(unsigned int frameNo);

	void VertexSkinning();
	void VertexSkinningByRange(const SkinningRange& range);

private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	PMXFileData _pmxFileData;
	VMDFileData _vmdFileData;

	NodeManager _nodeManager;

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
	std::unordered_map<std::wstring, std::vector<VMDIKkey>> _ikKeyMap;

	std::vector<std::wstring> _boneNodeNames;
	std::vector<BoneNodeLegacy*> _boneNodeByIdx;
	std::unordered_map<std::wstring, BoneNodeLegacy> _boneNodeByName;

	std::vector<IKSolver> _ikSolvers;

	DWORD _startTime = 0;

	std::vector<SkinningRange> _skinningRanges;
	std::vector<std::future<void>> _parallelUpdateFutures;
};

