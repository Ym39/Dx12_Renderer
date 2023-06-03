#pragma once
#include<d3d12.h>
#include<vector>
#include<wrl.h>
#include<memory>

class Dx12Wrapper;
class PMDActor;
class PMDRenderer
{
private:
	Dx12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12PipelineState> _pipeline = nullptr;
	ComPtr<ID3D12PipelineState> _shadowPipeline = nullptr;
	ComPtr<ID3D12RootSignature> _rootSignature = nullptr;

	HRESULT CreateGraphicsPipelineForPMD();

	HRESULT CreateRootSignature();

	bool CheckShaderComplieResult(HRESULT result, ID3DBlob* error = nullptr);

	std::vector<std::shared_ptr<PMDActor>> _actors;

public:
	PMDRenderer(Dx12Wrapper& dx12);
	~PMDRenderer();
	void Update();
	void PlayAnimation();

	void BeforeDrawFromLight();
	void DrawFromLight();

	void BeforeDraw();
	void Draw();

	void AddActor(std::shared_ptr<PMDActor> actor);

	ID3D12PipelineState* GetPipelineState();
	ID3D12PipelineState* GetShadowPipelineState();
	ID3D12RootSignature* GetRootSignature();

	Dx12Wrapper& GetDirect();
};

