#pragma once

#include<d3d12.h>
#include<DirectXMath.h>
#include<vector>
#include<unordered_map>
#include<string>
#include<wrl.h>

class Dx12Wrapper;
class PMDRenderer;
class PMDActor
{
	friend PMDRenderer;

private:
	PMDRenderer& _renderer;
	Dx12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	ComPtr<ID3D12Resource> _trasformMat = nullptr;
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;

	struct MaterialForHlsl
	{
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		DirectX::XMFLOAT3 specular;
		float specularity;
		DirectX::XMFLOAT3 ambient;
	};

	struct AdditionalMaterial
	{
		std::string texPath;
		int toonIdx;
		bool edgeFlg;
	};

	struct Material
	{
		unsigned int indicesNum;
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	struct Transform
	{
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	Transform _transform;
	Transform* _mappedTransform = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	std::vector<Material> _materials;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

	std::vector<DirectX::XMMATRIX> _boneMatrices;

	struct BoneNode
	{
		int boneIdx;
		DirectX::XMFLOAT3 startPos;
		DirectX::XMFLOAT3 endPos;
		std::vector<BoneNode*> children;
	};

	std::unordered_map<std::string, BoneNode> _boneNodeTable;

	HRESULT CreateMaterialData();

	ComPtr<ID3D12DescriptorHeap> _materialHeap = nullptr;

	HRESULT CreateMaterialAndTextureView();

	HRESULT CreateTransformView();

	HRESULT LoadPMDFile(const char* path);

	float _angle;

public:
	PMDActor(const char* filepath, PMDRenderer& renderer);
	~PMDActor();

	PMDActor* clone();
	void Update();
	void Draw();
};

