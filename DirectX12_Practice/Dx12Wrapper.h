#pragma once
#include<d3d12.h>
#include<dxgi1_6.h>
#include<map>
#include<memory>
#include<unordered_map>
#include<DirectXTex.h>
#include<wrl.h>
#include<string>
#include<functional>
#include<array>

class Transform;
class Dx12Wrapper
{
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	Dx12Wrapper(HWND hwnd);
	~Dx12Wrapper();

	void SetCameraSetting();
	void PreDrawStencil();
	void PreDrawReflection() const;
	void PreDrawShadow() const;
	void PreDrawToPera1() const;
	void DrawToPera1();
	void DrawToPera1ForFbx();
	void PostDrawToPera1();
	void DrawAmbientOcclusion();
	void DrawShrinkTextureForBlur();
	void DrawScreenSpaceReflection();
	void Clear();
	void Draw();
	void Update();
	void BeginDraw();
	void EndDraw();

	void SetRenderTargetByMainFrameBuffer() const;
	void SetRenderTargetSSRMaskBuffer() const;
	void SetShaderResourceSSRMaskBuffer(unsigned int rootParameterIndex) const;

	void SetSceneBuffer(int rootParameterIndex) const;
	void SetRSSetViewportsAndScissorRectsByScreenSize() const;

	void SetLightDepthTexture(int rootParameterIndex) const;

	void SetResolutionDescriptorHeap(unsigned int rootParameterIndex) const;
	void SetGlobalParameterBuffer(unsigned int rootParameterIndex) const;
	void SetPostProcessParameterBuffer(unsigned int rootParameterIndex) const;

	void ChangeResourceBarrierFinalRenderTarget(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) const;
	void SetFinalRenderTarget() const;
	void SetOnlyDepthBuffer() const;
	void ClearFinalRenderTarget() const;

	void ChangeToDepthWriteMainDepthBuffer() const;
	void ChangeToShaderResourceMainDepthBuffer() const;

	ComPtr<ID3D12Device> Device();
	ComPtr<ID3D12GraphicsCommandList> CommandList();
	ComPtr<IDXGISwapChain4> SwapChain();
	ComPtr<ID3D12DescriptorHeap> GetHeapForImgui();

	ComPtr<ID3D12Resource> GetTextureByPath(const char* texpath);
	ComPtr<ID3D12Resource> GetTextureByPath(const std::wstring& texpath);
	ComPtr<ID3D12Resource> GetWhiteTexture();
	ComPtr<ID3D12Resource> GetBlackTexture();
	ComPtr<ID3D12Resource> GetGradTexture();

	int GetBloomIteration() const;
	void SetBloomIteration(int iteration);
	float GetBloomIntensity() const;
	void SetBloomIntensity(float intensity);
	float GetScreenSpaceReflectionStepSize() const;
	void SetScreenSpaceReflectionStepSize(float stepSize);

	void SetFov(float fov);
	void SetDirectionalLightRotation(float vec[3]);
	const DirectX::XMFLOAT3& GetDirectionalLightRotation() const;

	int GetPostProcessingFlag() const;
	void SetPostProcessingFlag(int flag);

	DirectX::XMFLOAT3 GetCameraPosition() const;

private:

	struct BloomParameter
	{
		int iteration;
		float intensity;
		float screenSpaceReflectionStepSize;
	};

	struct ResolutionBuffer
	{
		float width;
		float height;
	};

	struct GlobalParameterBuffer
	{
		float time;
		int randomSeed;
	};

	HRESULT InitializeDXGIDevice();
	HRESULT InitializeCommand();
	HRESULT CreateSwapChain(const HWND& hwnd);
	HRESULT CreateFinalRenderTargets();
	HRESULT CreateSceneView();
	HRESULT CreateBloomParameterResource();
	HRESULT CreateResolutionConstantBuffer();
	HRESULT CreateGlobalParameterBuffer();
	HRESULT CreatePeraResource();
	bool CreatePeraVertex();
	bool CreatePeraPipeline();
	bool CreateSSRPipeline();
	bool CreateAmbientOcclusionBuffer();
	bool CreateAmbientOcclusionDescriptorHeap();
	void CreateTextureLoaderTable();
	HRESULT CreateDepthStencilView();
	ID3D12Resource* CreateWhiteTexture();
	ID3D12Resource* CreateBlackTexture();
	ID3D12Resource* CreateGrayGradationTexture();

	ID3D12Resource* CreateTextureFromFile(const char* texpath);
	ID3D12Resource* CreateTextureFromFile(const std::wstring& texpath);
	ID3D12Resource* CreateDefaultTexture(size_t width, size_t height);
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeapForImgui();

	void UpdateGlobalParameterBuffer() const; 

	SIZE mWindowSize;

	ComPtr<IDXGIFactory6> mDXGIFactory = nullptr;
	ComPtr<ID3D12Device> mDevice = nullptr;
	ComPtr<ID3D12CommandAllocator> mCmdAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList> mCmdList = nullptr;
	ComPtr<ID3D12CommandQueue> mCmdQueue = nullptr;
	ComPtr<IDXGISwapChain4> mSwapChain = nullptr;
	ComPtr<ID3D12DescriptorHeap> mRtvHeaps = nullptr;
	std::vector<ID3D12Resource*> mBackBuffers;
	std::unique_ptr<D3D12_VIEWPORT> mViewport;
	std::unique_ptr<D3D12_RECT> mScissorRect;

	ComPtr<ID3D12DescriptorHeap> mPeraRTVHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mPeraSRVHeap = nullptr;
	ComPtr<ID3D12Resource> mPeraVB;
	D3D12_VERTEX_BUFFER_VIEW mPeraVertexBufferView;
	ComPtr<ID3D12RootSignature> mPeraRootSignature;

	// SSR
	ComPtr<ID3D12RootSignature> mSsrRootSignature = nullptr;
	ComPtr<ID3D12PipelineState> mSsrPipeline = nullptr;
	ComPtr<ID3D12Resource> mSsrTexture = nullptr;

	// SceneBuffer(view, proj, shadow, camera ...)
	ComPtr<ID3D12Resource> mSceneConstBuff = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSceneDescHeap = nullptr;

	ComPtr<ID3D12Resource> mGlobalParameterBuffer = nullptr;

	ComPtr<ID3D12Resource> mResolutionConstBuffer = nullptr;
	ComPtr<ID3D12DescriptorHeap> mResolutionDescHeap = nullptr;
	ResolutionBuffer* mMappedResolutionBuffer = nullptr;

	ComPtr<ID3D12DescriptorHeap> mDepthStencilViewHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mDepthSRVHeap = nullptr;
	ComPtr<ID3D12Resource> mDepthBuffer = nullptr;
	ComPtr<ID3D12Resource> mLightDepthBuffer = nullptr;
	ComPtr<ID3D12Resource> mStencilBuffer = nullptr;

	std::array<ComPtr<ID3D12Resource>, 2> mPera1Resource; // frameTex, NormalTex
	std::array<ComPtr<ID3D12Resource>, 2> mBloomBuffer; // texHighLum, texShrinkHighLum
	ComPtr<ID3D12Resource> mReflectionBuffer;
	ComPtr<ID3D12Resource> mSsrMaskBuffer;

	ComPtr<ID3D12PipelineState> mAoPipeline;
	ComPtr<ID3D12Resource> mAoBuffer; // texSSAO
	ComPtr<ID3D12DescriptorHeap> mAoRenderTargetViewDescriptorHeap;
	ComPtr<ID3D12DescriptorHeap> mAoSRVDescriptorHeap;

	ComPtr<ID3D12PipelineState> mBlurShrinkPipeline;
	ComPtr<ID3D12Resource> mDofBuffer; // texShrink
	ComPtr<ID3D12Resource> mBloomResultTexture; //offset 8
	ComPtr<ID3D12Resource> mPostProcessParameterBuffer;
	BloomParameter* mMappedPostProcessParameter = nullptr;
	ComPtr<ID3D12DescriptorHeap> mPostProcessParameterSRVHeap;
	ComPtr<ID3D12RootSignature> mBlurResultRootSignature;
	ComPtr<ID3D12PipelineState> mBlurResultPipeline;
	int mBloomIteration = 0;

	ComPtr<ID3D12PipelineState> mScreenPipelineDefault;
	ComPtr<ID3D12PipelineState> mScreenPipelineBloom;
	ComPtr<ID3D12PipelineState> mScreenPipelineSSAO;
	ComPtr<ID3D12PipelineState> mScreenPipelineBloomSSAO;

	Transform* mCameraTransform;
	Transform* mDirectionalLightTransform;

	//Setting Value
	float mFov;
	float mLightVector[3];

	struct SceneMatricesData
	{
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX proj;
		DirectX::XMMATRIX invProj;
		DirectX::XMMATRIX lightCamera;
		DirectX::XMMATRIX shadow;
		DirectX::XMFLOAT4 light;
		DirectX::XMFLOAT3 eye;
	};

	ComPtr<ID3D12Fence> mFence = nullptr;
	UINT64 mFenceVal = 0;

	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map<std::string, LoadLambda_t> mLoadLambdaTable;

	std::map<std::string, ComPtr<ID3D12Resource>> mResourceTable;
	std::map<std::wstring, ComPtr<ID3D12Resource>> mResourceTableW;

	ComPtr<ID3D12Resource> mWhiteTex = nullptr;
	ComPtr<ID3D12Resource> mBlackTex = nullptr;
	ComPtr<ID3D12Resource> mGradTex = nullptr;

	ComPtr<ID3D12DescriptorHeap> mHeapForImgui;

	int mCurrentPPFlag;
};

