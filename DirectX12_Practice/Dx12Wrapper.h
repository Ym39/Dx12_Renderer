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
	void PreDrawShadow();
	bool PreDrawToPera1();
	void DrawToPera1();
	void DrawToPera1ForFbx();
	void PostDrawToPera1();
	void DrawAmbientOcclusion();
	void DrawShrinkTextureForBlur();
	void Clear();
	void Draw();
	void Update();
	void BeginDraw();
	void EndDraw();

	ComPtr<ID3D12Device> Device();
	ComPtr<ID3D12GraphicsCommandList> CommandList();
	ComPtr<IDXGISwapChain4> Swapchain();
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

	void SetFov(float fov);
	void SetDirectionalLightRotation(float vec[3]);
	const DirectX::XMFLOAT3& GetDirectionalLightRotation() const;

	int GetPostProcessingFlag() const;
	void SetPostProcessingFlag(int flag);

	DirectX::XMFLOAT3 GetCameraPosition() const;
	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetProjectionMatrix() const;

private:

	struct BloomParameter
	{
		int iteration;
		float intensity;
		DirectX::XMFLOAT2 padding;
	};

	HRESULT InitializeDXGIDevice();
	HRESULT InitializeCommand();
	HRESULT CreateSwapChain(const HWND& hwnd);
	HRESULT CreateFinalRenderTargets();
	HRESULT CreateSceneView();
	HRESULT CreateBloomParameterResource();
	HRESULT CreatePeraResource();
	bool CreatePeraVertex();
	bool CreatePeraPipeline();
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

	SIZE _winSize;

	ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
	ComPtr<ID3D12Device> _dev = nullptr;
	ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;
	ComPtr<IDXGISwapChain4> _swapChain = nullptr;
	ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	std::vector<ID3D12Resource*> _backBuffers;
	std::unique_ptr<D3D12_VIEWPORT> _viewport;
	std::unique_ptr<D3D12_RECT> _scissorrect;

	ComPtr<ID3D12DescriptorHeap> _peraRTVHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> _peraSRVHeap = nullptr;
	ComPtr<ID3D12Resource> _peraVB;
	D3D12_VERTEX_BUFFER_VIEW _peraVBV;
	ComPtr<ID3D12RootSignature> _peraRS;

	// SceneBuffer(view, proj, shadow, camera ...)
	ComPtr<ID3D12Resource> _sceneConstBuff = nullptr;
	ComPtr<ID3D12DescriptorHeap> _sceneDescHeap = nullptr;

	ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> _depthSRVHeap = nullptr;
	ComPtr<ID3D12Resource> _depthBuffer = nullptr;
	ComPtr<ID3D12Resource> _lightDepthBuffer = nullptr;

	std::array<ComPtr<ID3D12Resource>, 2> _pera1Resource; // frameTex, NormalTex
	std::array<ComPtr<ID3D12Resource>, 2> _bloomBuffer; // texHighLum, texShrinkHighLum

	ComPtr<ID3D12PipelineState> _aoPipeline;
	ComPtr<ID3D12Resource> _aoBuffer; // texSSAO
	ComPtr<ID3D12DescriptorHeap> _aoRTVDH;
	ComPtr<ID3D12DescriptorHeap> _aoSRVDH;

	ComPtr<ID3D12PipelineState> _blurShrinkPipeline;
	ComPtr<ID3D12Resource> _dofBuffer; // texShrink
	ComPtr<ID3D12Resource> _bloomResultTexture; //offset 8
	ComPtr<ID3D12Resource> _bloomResultParameterBuffer;
	BloomParameter* _mappedBloomParameter = nullptr;
	ComPtr<ID3D12DescriptorHeap> _bloomResultSRVHeap;
	ComPtr<ID3D12RootSignature> _blurResultRootSignature;
	ComPtr<ID3D12PipelineState> _blurResultPipeline;
	int mBloomIteration = 0;

	ComPtr<ID3D12PipelineState> _screenPipelineDefault;
	ComPtr<ID3D12PipelineState> _screenPipelineBloom;
	ComPtr<ID3D12PipelineState> _screenPipelineSSAO;
	ComPtr<ID3D12PipelineState> _screenPipelineBloomSSAO;

	Transform* _cameraTransform;
	Transform* _directionalLightTransform;

	//Setting Value
	float _fov;
	float _lightVector[3];

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

	SceneMatricesData* _mappedSceneMatricesData;
	ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _fenceVal = 0;

	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map<std::string, LoadLambda_t> _loadLambdaTable;

	std::map<std::string, ComPtr<ID3D12Resource>> _resourceTable;
	std::map<std::wstring, ComPtr<ID3D12Resource>> _resourceTableW;

	ComPtr<ID3D12Resource> _whiteTex = nullptr;
	ComPtr<ID3D12Resource> _blackTex = nullptr;
	ComPtr<ID3D12Resource> _gradTex = nullptr;

	ComPtr<ID3D12DescriptorHeap> _heapForImgui;

	int _currentPPFlag;
};

