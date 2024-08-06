#pragma once
#include<d3d12.h>
#include<vector>
#include<wrl.h>
#include<memory>
#include<DirectXMath.h>

class Dx12Wrapper;
class PMXActor;
class PMXRenderer
{
private:
	Dx12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12PipelineState> _deferredPipeline = nullptr;
	ComPtr<ID3D12PipelineState> _onlyDepthPipeline = nullptr;
	ComPtr<ID3D12PipelineState> _notDepthWriteForwardPipeline = nullptr;
	ComPtr<ID3D12PipelineState> _forwardPipeline = nullptr;
	ComPtr<ID3D12PipelineState> _shadowPipeline = nullptr;
	ComPtr<ID3D12PipelineState> _reflectionPipeline = nullptr;
	ComPtr<ID3D12RootSignature> _rootSignature = nullptr;

	struct ParameterBuffer
	{
		float bloomThreshold;
		DirectX::XMFLOAT3 padding;
	};

	ComPtr<ID3D12Resource> _parameterBuffer = nullptr;
	ComPtr<ID3D12DescriptorHeap> _parameterHeap = nullptr;
	ParameterBuffer* _mappedParameterBuffer = nullptr;

	HRESULT CreateGraphicsPipelineForPMX();

	HRESULT CreateRootSignature();

	HRESULT CreateParameterBufferAndHeap();

	bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error = nullptr);

	std::vector<std::shared_ptr<PMXActor>> _actors;


public:
	PMXRenderer(Dx12Wrapper& dx12);
	~PMXRenderer();
	void Update();

	void BeforeDrawFromLight() const;
	void BeforeDrawAtForwardPipeline();
	void BeforeDrawAtDeferredPipeline();
	void BeforeDrawReflection();

	void DrawFromLight() const;
	void Draw() const;
	void DrawOnlyDepth() const;
	void DrawForwardNotDepthWrite() const;
	void DrawReflection() const;

	void AddActor(std::shared_ptr<PMXActor> actor);
	const PMXActor* GetActor();

	void SetBloomThreshold(float threshold);
	float GetBloomThreshold() const;

	ID3D12PipelineState* GetPipelineState();
	ID3D12RootSignature* GetRootSignature();

	Dx12Wrapper& GetDirect() const;
};

