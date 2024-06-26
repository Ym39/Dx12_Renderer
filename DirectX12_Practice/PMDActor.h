#pragma once

#include<d3d12.h>
#include<DirectXMath.h>
#include<vector>
#include<array>
#include<unordered_map>
#include<string>
#include<wrl.h>
#include<timeapi.h>
#include<algorithm>
#include<sstream>

class Dx12Wrapper;
class PMDActor
{
public:
	PMDActor(const char* filepath, Dx12Wrapper& dx);
	~PMDActor();
	PMDActor* clone();

	void Update();
	void PlayAnimation();
	void Draw(Dx12Wrapper& dx, bool isShadow);
	void SetPosition(float x, float y, float z);

private:
	struct PMDIK;
	struct BoneNode;
	struct Material;
	struct Motion;
	struct Transform
	{
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};
	struct VMDIKEnable;

	//Init
	HRESULT LoadPMDFile(const char* path, Dx12Wrapper& dx);
	void LoadVMDFile(const char* filepath);
	void LoadVMDIKFile(const char* filepath);
	HRESULT CreateTransformView(Dx12Wrapper& dx);
	HRESULT CreateMaterialData(Dx12Wrapper& dx);
	HRESULT CreateMaterialAndTextureView(Dx12Wrapper& dx);

	//Update
	void MotionUpdate();
	void IKSolve(int frameNo);
	void SolveCCDIK(const PMDIK& ik);
	void SolveCosineIK(const PMDIK& ik);
	void SolveLookAt(const PMDIK& ik);
	float GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n);
	void RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat);

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	ComPtr<ID3D12Resource> _transformMat = nullptr;
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;

	unsigned int _vertNum;
	unsigned int _indexNum;

	DirectX::XMFLOAT3 _position;

	Transform _transform;
	Transform* _mappedTransform = nullptr;
	DirectX::XMMATRIX* _mappedMatrices = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	std::vector<Material> _materials;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

	std::vector<DirectX::XMMATRIX> _boneMatrices;
	std::vector<PMDIK> _ikData;

	std::unordered_map<std::string, BoneNode> _boneNodeTable;
	std::unordered_map<int, BoneNode> _boneNodeTableByIdx;

	std::vector<std::string> _boneNameArray;
	std::vector<BoneNode*> _boneNodeAddressArray;

	ComPtr<ID3D12DescriptorHeap> _materialHeap = nullptr;

	std::unordered_map<std::string, std::vector<Motion>> _motionData;

	unsigned int _duration = 0;

	std::vector<uint32_t> _kneeIdxes;

	DWORD _startTime;
	float _angle;

	std::vector<VMDIKEnable> _ikEnableData;

	struct BoneNode
	{
		uint32_t boneIdx;
		uint32_t boneType;
		uint32_t parentBone;
		uint32_t ikParentBone;
		DirectX::XMFLOAT3 startPos;
		std::vector<BoneNode*> children;
	};

	struct PMDIK
	{
		uint16_t boneIdx;
		uint16_t targetIdx;
		uint16_t iterations;
		float limit;
		std::vector<uint16_t> nodeIdx;
	};

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

	struct Motion
	{
		unsigned int frameNo;
		DirectX::XMVECTOR quaternion;
		DirectX::XMFLOAT3 offset;
		DirectX::XMFLOAT2 p1, p2;

		Motion(unsigned int fno, DirectX::XMVECTOR& q,
			DirectX::XMFLOAT3 ofst,
			const DirectX::XMFLOAT2& ip1, const DirectX::XMFLOAT2& ip2)
			:frameNo(fno), quaternion(q),
			offset(ofst),
			p1(ip1),
			p2(ip2)
		{}
	};

	struct VMDIKEnable
	{
		uint32_t frameNo;

		std::unordered_map<std::string, bool> ikEnableTable;
	};
};

