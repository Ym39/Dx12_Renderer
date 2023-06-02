#pragma once
#include<d3d12.h>
#include<vector>
#include<wrl.h>
#include<memory>

class Dx12Wrapper;
class PMDActor;
class PMDRenderer
{
	friend PMDActor;
private:
	Dx12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12PipelineState> _pipeline = nullptr;
	ComPtr<ID3D12PipelineState> _shadowPipeline = nullptr;
	ComPtr<ID3D12RootSignature> _rootSignature = nullptr;

	ComPtr<ID3D12Resource> _whiteTex = nullptr;
	ComPtr<ID3D12Resource> _blackTex = nullptr;
	ComPtr<ID3D12Resource> _gradTex = nullptr;

	ID3D12Resource* CreateDefaultTexture(size_t width, size_t height);
	ID3D12Resource* CreateWhiteTexture();
	ID3D12Resource* CreateBlackTexture();
	ID3D12Resource* CreateGrayGradiationTexture();

	HRESULT CreateGraphicsPipelineForPMD();

	HRESULT CreateRootSignature();

	bool CheckShaderComplieResult(HRESULT result, ID3DBlob* error = nullptr);

public:
	PMDRenderer(Dx12Wrapper& dx12);
	~PMDRenderer();
	void Update();

	void BeforeDrawFromLight();
	void DrawFromLight();

	void BeforeDraw();
	void Draw();

	ID3D12PipelineState* GetPipelineState();
	ID3D12PipelineState* GetShadowPipelineState();
	ID3D12RootSignature* GetRootSignature();
};

