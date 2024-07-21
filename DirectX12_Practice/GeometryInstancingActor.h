#pragma once
#include <vector>
#include <wrl/client.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <memory>

#include "IActor.h"
#include "IGetTransform.h"

#include "CubeGeometry.h"

class GeometryInstancingActor : public IActor,
                                public IGetTransform
{
public:
	GeometryInstancingActor(const CubeGeometry& cube, unsigned int instancingCount);
	virtual ~GeometryInstancingActor() override;

 	// override
	void Initialize(Dx12Wrapper& dx) override;
	void Draw(Dx12Wrapper& dx, bool isShadow) const override;
	void Update() override;
	void EndOfFrame(Dx12Wrapper& dx) override;
	Transform& GetTransform() override;
	int GetIndexCount() const override;
	std::string GetName() const override;
	void SetName(std::string name) override;
	void UpdateImGui(Dx12Wrapper& dx) override;

private:
	struct GeometryInstanceData
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 rotation;
		DirectX::XMFLOAT3 scale;
		DirectX::XMFLOAT4 color;

		GeometryInstanceData(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& rotation, const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT4& color)
		{
			this->position = position;
			this->rotation = rotation;
			this->scale = scale;
			this->color = color;
		}
	};

	void InitializeInstanceData(unsigned int instanceCount);

	HRESULT CreateVertexBufferAndIndexBuffer(Dx12Wrapper& dx);
	HRESULT CreateInstanceBuffer(Dx12Wrapper& dx);
	HRESULT UploadInstanceData(Dx12Wrapper& dx);
	void ChangeInstanceCount(Dx12Wrapper& dx);

private:
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	std::string mName;

	const std::vector<Vertex>& mVertices;
	const std::vector<unsigned int>& mIndices;

	ComPtr<ID3D12Resource> mVertexBuffer = nullptr;
	ComPtr<ID3D12Resource> mIndexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW mIndexBufferView = {};
	Vertex* mMappedVertex = nullptr;
	unsigned int* mMappedIndex = nullptr;

	ComPtr<ID3D12Resource> mInstanceBuffer = nullptr;
	ComPtr<ID3D12Resource> mInstanceUploadBuffer = nullptr;

	unsigned int mInstanceCount;
	float mInstanceUnit;
	std::vector<GeometryInstanceData> mInstanceData{};

	std::unique_ptr<Transform> mTransform{};

	bool mRefreshInstanceBufferFlag = false;
};

