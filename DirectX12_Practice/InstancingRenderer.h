#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <memory>

class Dx12Wrapper;
class GeometryInstancingActor;

class InstancingRenderer
{
public:
	InstancingRenderer(Dx12Wrapper& dx);
	~InstancingRenderer();

	void Update();
	void BeforeDrawAtForwardPipeline();
	void Draw();
	void EndOfFrame();
	void AddActor(std::shared_ptr<GeometryInstancingActor> actor);

private:
	HRESULT CreateRootSignature();
	HRESULT CreateGraphicsPipeline();
	bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error = nullptr);

private:
	Dx12Wrapper& mDirectX;

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12PipelineState> mForwardPipeline = nullptr;

	std::vector<std::shared_ptr<GeometryInstancingActor>> mActorList = {};
};

