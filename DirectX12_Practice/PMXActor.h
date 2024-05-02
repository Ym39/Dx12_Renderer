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
#include "MorphManager.h"
#include "PhysicsManager.h"
#include "IGetTransform.h"
#include "Transform.h"

using namespace DirectX;


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

struct UpdateRange
{
	unsigned int startIndex;
	unsigned int range;
};

struct LoadMaterial
{
	bool visible;
	std::string name;
	XMFLOAT4 diffuse;
	XMFLOAT3 specular;
	float specularPower;
	XMFLOAT3 ambient;
};

class NodeManager;
class MorphManager;

class Dx12Wrapper;
class PMXActor : IGetTransform
{
public:
	PMXActor();
	~PMXActor();

	bool Initialize(const std::wstring& filePath, Dx12Wrapper& dx);
	void Update();
	void UpdateAnimation();
	void Draw(Dx12Wrapper& dx, bool isShadow);

	const std::vector<LoadMaterial>& GetMaterials() const;
	void SetMaterials(const std::vector<LoadMaterial>& setMaterials);

	Transform& GetTransform() override;

private:
	HRESULT CreateVbAndIb(Dx12Wrapper& dx);
	HRESULT CreateTransformView(Dx12Wrapper& dx);
	HRESULT CreateMaterialData(Dx12Wrapper& dx);
	HRESULT CreateMaterialAndTextureView(Dx12Wrapper& dx);

	void LoadVertexData(const std::vector<PMXVertex>& vertices);

	void InitAnimation(VMDFileData& vmdFileData);

	void InitPhysics(const PMXFileData& pmxFileData);

	void InitParallelVertexSkinningSetting();

	void VertexSkinning();
	void VertexSkinningByRange(const SkinningRange& range);

	void MorphMaterial();
	void MorphBone();

	void ResetPhysics();
	void UpdatePhysicsAnimation(DWORD elapse);

private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	PMXFileData _pmxFileData;
	VMDFileData _vmdFileData;

	NodeManager _nodeManager;
	MorphManager _morphManager;

	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	UploadVertex* _mappedVertex;
	std::vector<UploadVertex> _uploadVertices;

	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;
	std::vector<ComPtr<ID3D12Resource>> _sphereTextureResources;

	Transform _tranform;
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
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	ComPtr<ID3D12DescriptorHeap> _materialHeap = nullptr;
	char* _mappedMaterial = nullptr;
	std::vector<LoadMaterial> _loadedMaterial;

	struct MaterialForShader
	{
		XMFLOAT4 diffuse;
		XMFLOAT3 specular;
		float specularPower;
		XMFLOAT3 ambient;
	};

	unsigned int _duration;
	unsigned int _startTime = 0;

	std::vector<SkinningRange> _skinningRanges;
	std::vector<std::future<void>> _parallelUpdateFutures;

	std::vector<UpdateRange> _morphMaterialRanges;
	std::vector<std::future<void>> _parallelMorphMaterialUpdateFutures;

	std::vector<UpdateRange> _morphBoneRanges;
	std::vector<std::future<void>> _parallelMorphBoneUpdateFutures;

	std::vector<std::unique_ptr<RigidBody>> _rigidBodies;
	std::vector<std::unique_ptr<Joint>> _joints;
};

