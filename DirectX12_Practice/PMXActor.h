#pragma once
#include <string>
#include <vector>
#include<DirectXMath.h>
#include<d3d12.h>
#include<wrl.h>
#include<codecvt>
#include<locale>

using namespace DirectX;

class Dx12Wrapper;
class PMXRenderer;
class PMXActor
{
	friend PMXRenderer;
private:
	PMXRenderer& _renderer;
	Dx12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	ComPtr<ID3D12Resource> _trasformMat = nullptr;
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;

	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

public:

	PMXActor(const std::wstring& _filePath, PMXRenderer& renderer);

	struct PMXModelData
	{
		static constexpr int NO_DATA_FLAG = -1;

		struct Vertex
		{
			XMFLOAT3 position;
			XMFLOAT3 normal;
			XMFLOAT2 uv;
			std::vector<XMFLOAT4> additionalUV;

			struct Weight
			{
				enum Type
				{
					BDEF1,
					BDEF2,
					BDEF4,
					SDEF,
				};

				Type type;
				int born1;
				int	born2;
				int	born3;
				int	born4;
				float weight1;
				float weight2;
				float weight3;
				float weight4;
				XMFLOAT3 c;
				XMFLOAT3 r0;
				XMFLOAT3 r1;
			} weight;

			float edgeMagnif;
		};

		struct VertexForShader
		{
			XMFLOAT3 position;
			XMFLOAT3 normal;
			XMFLOAT2 uv;
		};

		struct Surface
		{
			int vertexIndex;
		};

		struct Material
		{
			XMFLOAT4 diffuse;
			XMFLOAT3 specular;
			float specularity;
			XMFLOAT3 ambient;

			int colorMapTextureIndex;
			int toonTextureIndex;
			// 스피어 텍스쳐 비대응

			// 메터리얼 당 정점 수
			int vertexNum;
		};

		struct Bone
		{
			std::wstring name;
			// English version
			std::string nameEnglish;
			XMFLOAT3 position;
			int parentIndex;
			int transformationLevel;
			unsigned short flag;
			XMFLOAT3 coordOffset;
			int childrenIndex;
			int impartParentIndex;
			float impartRate;
			// 固定軸方向ベクトル
			XMFLOAT3 fixedAxis;
			// ローカルのX軸方向ベクトル
			XMFLOAT3 localAxisX;
			// ローカルのZ軸方向ベクトル
			XMFLOAT3 localAxisZ;
			int externalParentKey;
			int ikTargetIndex;
			int ikLoopCount;
			float ikUnitAngle;
			struct IKLink
			{
				int index;
				bool existAngleLimited;
				XMFLOAT3 limitAngleMin;
				XMFLOAT3 limitAngleMax;
			};
			std::vector<IKLink> ikLinks;
		};


		std::vector<Vertex> vertices;
		std::vector<VertexForShader> verticesForShader;
		std::vector<unsigned short> indices;
		std::vector<std::wstring> texturePaths;
		std::vector<Material> materials;
		std::vector<Bone> bones;
	};

	bool loadPMX(PMXModelData& data, const std::wstring& _filePath);

	HRESULT CreateVbAndIb();
private:
	PMXModelData _modelData;
};

