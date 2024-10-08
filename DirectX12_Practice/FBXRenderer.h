#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl.h>
#include <memory>
#include <DirectXMath.h>

class Dx12Wrapper;
class FBXActor;

class FBXRenderer
{
public:
	FBXRenderer(Dx12Wrapper& dx);
	~FBXRenderer();

	void Update();

	void BeforeWriteToStencil();
	void BeforeDrawAtForwardPipeline();

	void Draw();

	ID3D12PipelineState* GetPipelineState() const;
	ID3D12RootSignature* GetRootSignature() const;
	void AddActor(std::shared_ptr<FBXActor> actor);
	std::vector<std::shared_ptr<FBXActor>>& GetActor();

private:
	HRESULT CreateRootSignature();
	HRESULT CreateGraphicsPipeline();
	bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error = nullptr);

private:
	Dx12Wrapper& mDx;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12PipelineState> mForwardPipeline = nullptr;
	ComPtr<ID3D12PipelineState> mStencilWritePipeline = nullptr;

	std::vector<std::shared_ptr<FBXActor>> mActors;
};

