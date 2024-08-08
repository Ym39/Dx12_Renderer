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
#include "IActor.h"

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
	bool isTransparent;
};

class NodeManager;
class MorphManager;

class Dx12Wrapper;
class PMXActor : public IGetTransform,
                 public IActor
{
public:
	PMXActor();
	~PMXActor();

	bool Initialize(const std::wstring& filePath, Dx12Wrapper& dx);
	void Update();
	void UpdateAnimation();
	void Draw(Dx12Wrapper& dx, bool isShadow) const;
	void DrawReflection(Dx12Wrapper& dx) const;
	void DrawOpaque(Dx12Wrapper& dx) const;

	const std::vector<LoadMaterial>& GetMaterials() const;
	void SetMaterials(const std::vector<LoadMaterial>& setMaterials);

	Transform& GetTransform() override;
	std::string GetName() const override;
	void SetName(std::string name) override;
	void UpdateImGui(Dx12Wrapper& dx) override;

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

	PMXFileData mPmxFileData;
	VMDFileData mVmdFileData;

	NodeManager mNodeManager;
	MorphManager mMorphManager;

	ComPtr<ID3D12Resource> mVertexBuffer = nullptr;
	ComPtr<ID3D12Resource> mIndexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW mIndexBufferView = {};

	UploadVertex* mMappedVertex;
	std::vector<UploadVertex> mUploadVertices;

	std::vector<ComPtr<ID3D12Resource>> mTextureResources;
	std::vector<ComPtr<ID3D12Resource>> mToonResources;
	std::vector<ComPtr<ID3D12Resource>> mSphereTextureResources;

	Transform mTransform;
	ComPtr<ID3D12Resource> mTransformMat = nullptr;
	ComPtr<ID3D12DescriptorHeap> mTransformHeap = nullptr;
	std::vector<XMMATRIX> mBoneMatrices;
	std::vector<XMMATRIX> mBoneLocalMatrices;

	DirectX::XMMATRIX* mMappedMatrices;
	DirectX::XMMATRIX* mMappedReflectionMatrices;
	ComPtr<ID3D12Resource> mTransformBuff = nullptr;
	ComPtr<ID3D12Resource> mReflectionTransformBuff = nullptr;

	ComPtr<ID3D12Resource> mMaterialBuff = nullptr;
	ComPtr<ID3D12DescriptorHeap> mMaterialHeap = nullptr;
	char* mMappedMaterial = nullptr;
	std::vector<LoadMaterial> mLoadedMaterial;

	struct MaterialForShader
	{
		XMFLOAT4 diffuse;
		XMFLOAT3 specular;
		float specularPower;
		XMFLOAT3 ambient;
	};

	unsigned int mDuration;
	unsigned int mStartTime = 0;

	std::vector<SkinningRange> mSkinningRanges;
	std::vector<std::future<void>> mParallelUpdateFutures;

	std::vector<UpdateRange> mMorphMaterialRanges;
	std::vector<std::future<void>> mParallelMorphMaterialUpdateFutures;

	std::vector<UpdateRange> mMorphBoneRanges;
	std::vector<std::future<void>> mParallelMorphBoneUpdateFutures;

	std::vector<std::unique_ptr<RigidBody>> mRigidBodies;
	std::vector<std::unique_ptr<Joint>> mJoints;

	std::string mName;
};

