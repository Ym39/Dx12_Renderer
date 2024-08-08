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

	ComPtr<ID3D12PipelineState> mDeferredPipeline = nullptr;
	ComPtr<ID3D12PipelineState> mOnlyDepthPipeline = nullptr;
	ComPtr<ID3D12PipelineState> mNotDepthWriteForwardPipeline = nullptr;
	ComPtr<ID3D12PipelineState> mForwardPipeline = nullptr;
	ComPtr<ID3D12PipelineState> mShadowPipeline = nullptr;
	ComPtr<ID3D12PipelineState> mReflectionPipeline = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	struct ParameterBuffer
	{
		float bloomThreshold;
		DirectX::XMFLOAT3 padding;
	};

	ComPtr<ID3D12Resource> mParameterBuffer = nullptr;
	ComPtr<ID3D12DescriptorHeap> mParameterHeap = nullptr;
	ParameterBuffer* mMappedParameterBuffer = nullptr;

	HRESULT CreateGraphicsPipelineForPMX();

	HRESULT CreateRootSignature();

	HRESULT CreateParameterBufferAndHeap();

	bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error = nullptr);

	std::vector<std::shared_ptr<PMXActor>> mActors;


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

