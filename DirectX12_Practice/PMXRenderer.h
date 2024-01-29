#pragma once
#include<d3d12.h>
#include<vector>
#include<wrl.h>
#include<memory>

class Dx12Wrapper;
class PMXActor;
class PMXRenderer
{
private:
	Dx12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12PipelineState> _pipeline = nullptr;
	ComPtr<ID3D12PipelineState> _shadowPipeline = nullptr;
	ComPtr<ID3D12RootSignature> _rootSignature = nullptr;

	HRESULT CreateGraphicsPipelineForPMX();

	HRESULT CreateRootSignature();

	bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error = nullptr);

	std::vector<std::shared_ptr<PMXActor>> _actors;

public:
	PMXRenderer(Dx12Wrapper& dx12);
	~PMXRenderer();
	void Update();

	void BeforeDrawFromLight();
	void BeforeDraw();

	void DrawFromLight();
	void Draw();

	void AddActor(std::shared_ptr<PMXActor> actor);
	const PMXActor* GetActor();

	ID3D12PipelineState* GetPipelineState();
	ID3D12RootSignature* GetRootSignature();

	Dx12Wrapper& GetDirect() const;
};

