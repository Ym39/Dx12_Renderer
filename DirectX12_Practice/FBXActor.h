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
                 public IType
{
public:
	FBXActor();

	bool Initialize(const std::string& path, Dx12Wrapper& dx);
	void Draw(Dx12Wrapper& dx, bool isShadow);
	void Update();

	Transform& GetTransform() override;
	TypeIdentity GetType() override;
	bool TestSelect(int mouseX, int mouseY, DirectX::XMFLOAT3 cameraPosition, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix) override;

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

	std::vector<FBXVertex> _vertices;
	std::vector<unsigned int> _indices;
	std::vector<FBXMesh> _meshes;
	unsigned int _vertexCount = 0;
	unsigned int _indexCount = 0;

	ComPtr<ID3D12Resource> _vertexBuffer = nullptr;
	ComPtr<ID3D12Resource> _indexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW _indexBufferView = {};
	FBXVertex* _mappedVertex;
	unsigned int* _mappedIndex;

	ComPtr<ID3D12Resource> _transformBuffer = nullptr;
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;
	DirectX::XMMATRIX* _mappedWorldTranform;
	Transform _transform;

	BoundsBox* _bounds = nullptr;
};

