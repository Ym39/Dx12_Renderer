#pragma once
#include <d3d12.h>
#include <memory>
#include <wrl/client.h>

#include "IActor.h"
#include "IGetTransform.h"
#include "IGeometry.h"

class GeometryActor : public IActor,
                      public IGetTransform
{
public:
    GeometryActor(const IGeometry& geometry);
    ~GeometryActor() override = default;

    void Initialize(Dx12Wrapper& dx);
    void Draw(Dx12Wrapper& dx) const;
    void Update();
    void EndOfFrame(Dx12Wrapper& dx);

    // override
    Transform& GetTransform() override;
    std::string GetName() const override;
    void SetName(std::string name)override;
    void UpdateImGui(Dx12Wrapper& dx) override;

private:
    HRESULT CreateVertexBufferAndIndexBuffer(Dx12Wrapper& dx);
    HRESULT CreateTransformBuffer(Dx12Wrapper& dx);

private:
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    const std::vector<Vertex>& mVertices;
    const std::vector<unsigned int>& mIndices;
    ComPtr<ID3D12Resource> mVertexBuffer = nullptr;
    ComPtr<ID3D12Resource> mIndexBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW mVertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW mIndexBufferView = {};
    Vertex* mMappedVertex = nullptr;
    unsigned int* mMappedIndex = nullptr;

    ComPtr<ID3D12Resource> mTransformBuffer = nullptr;
    std::unique_ptr<Transform> mTransform{};

    std::string mName;
};

