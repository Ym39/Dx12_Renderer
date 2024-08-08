#pragma once
#include <string>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl/client.h>

#include "Transform.h"
#include "IGetTransform.h"
#include "ISelectable.h"
#include "IType.h"
#include "Serialize.h"
#include "IActor.h"

class Dx12Wrapper;
class BoundsBox;

struct FBXVertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 uv;
};

struct FBXMesh
{
	unsigned int startVertex;
	unsigned int vertexCount;
	unsigned int startIndex;
	unsigned int indexCount;
};

class FBXActor : public IGetTransform,
                 public ISelectable,
                 public IType,
                 public IActor
{
public:
	FBXActor();

	bool Initialize(const std::string& path, Dx12Wrapper& dx);
	void SetMaterialName(const std::vector<std::string> materialNameList);
	void Draw(Dx12Wrapper& dx, bool isShadow) const;
	void Update();

	Transform& GetTransform() override;
	TypeIdentity GetType() override;
	bool TestSelect(int mouseX, int mouseY, DirectX::XMFLOAT3 cameraPosition, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix) override;

	std::string GetName() const override;
	void SetName(std::string name) override;
	void UpdateImGui(Dx12Wrapper& dx) override;

	const std::vector<std::string> GetMaterialNameList() const;
	void GetSerialize(json& j);

private:
	DirectX::XMFLOAT3 AiVector3ToXMFLOAT3(const aiVector3D& vec);
	DirectX::XMFLOAT2 AiVector2ToXMFLOAT2(const aiVector2D& vec);
	void ReadNode(aiNode* node, const aiScene* scene);
	void ReadMesh(aiMesh* mesh, const aiScene* scene);

	HRESULT CreateVertexBufferAndIndexBuffer(Dx12Wrapper& dx);
	HRESULT CreateTransformView(Dx12Wrapper& dx);

private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	std::string mModelPath;

	std::vector<FBXVertex> mVertices;
	std::vector<unsigned int> mIndices;
	std::vector<FBXMesh> mMeshes;
	std::vector<std::string> mMeshMaterialNameList;
	unsigned int mVertexCount = 0;
	unsigned int mIndexCount = 0;

	ComPtr<ID3D12Resource> mVertexBuffer = nullptr;
	ComPtr<ID3D12Resource> mIndexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW mIndexBufferView = {};
	FBXVertex* mMappedVertex;
	unsigned int* mMappedIndex;

	ComPtr<ID3D12Resource> mTransformBuffer = nullptr;
	ComPtr<ID3D12DescriptorHeap> mTransformHeap = nullptr;
	DirectX::XMMATRIX* mMappedWorldTransform;
	Transform mTransform;
	std::string mName;

	BoundsBox* mBounds = nullptr;
};

