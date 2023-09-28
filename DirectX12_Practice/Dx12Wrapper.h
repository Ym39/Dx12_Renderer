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

class Dx12Wrapper
{
	SIZE _winSize;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
	ComPtr<IDXGISwapChain4> _swapChain = nullptr;

	ComPtr<ID3D12Device> _dev = nullptr;
	ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;

	ComPtr<ID3D12Resource> _depthBuffer = nullptr;
	std::vector<ID3D12Resource*> _backBuffers;
	ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> _depthSRVHeap = nullptr;
	std::unique_ptr<D3D12_VIEWPORT> _viewport;
	std::unique_ptr<D3D12_RECT> _scissorrect;

	ComPtr<ID3D12Resource> _sceneConstBuff = nullptr;

	std::array<ComPtr<ID3D12Resource>, 2> _pera1Resource;
	ComPtr<ID3D12Resource> _peraResource2 = nullptr;
	ComPtr<ID3D12DescriptorHeap> _peraRTVHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> _peraSRVHeap = nullptr;

	ComPtr<ID3D12Resource> _peraVB;
	D3D12_VERTEX_BUFFER_VIEW _peraVBV;

	ComPtr<ID3D12PipelineState> _peraPipeline;
	ComPtr<ID3D12PipelineState> _peraPipeline2;
	ComPtr<ID3D12PipelineState> _blurPipeline;
	ComPtr<ID3D12RootSignature> _peraRS;

	ComPtr<ID3D12Resource> _bokehParamResource;

	ComPtr<ID3D12Resource> _lightDepthBuffer = nullptr;

	std::array<ComPtr<ID3D12Resource>, 2> _bloomBuffer;

	ComPtr<ID3D12Resource> _dofBuffer;

	ComPtr<ID3D12Resource> _aoBuffer;
	ComPtr<ID3D12PipelineState> _aoPipeline;

	ComPtr<ID3D12DescriptorHeap> _aoRTVDH;
	ComPtr<ID3D12DescriptorHeap> _aoSRVDH;

	DirectX::XMFLOAT3 _eye;
	DirectX::XMFLOAT3 _target;
	DirectX::XMFLOAT3 _up;

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

	DirectX::XMFLOAT3 _parallelLightVec;

	SceneMatricesData* _mappedSceneMatricesData;
	ComPtr<ID3D12DescriptorHeap> _sceneDescHeap = nullptr;

	ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _fenceVal = 0;

	HRESULT CreateFinalRenderTargets();

	HRESULT CreateDepthStencilView();

	HRESULT CreatePeraResource();

	HRESULT CreateBokehParamResource();

	HRESULT CreateSwapChain(const HWND& hwnd);

	HRESULT InitializeDXGIDevice();

	HRESULT InitializeCommand();

	HRESULT CreateSceneView();

	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map<std::string, LoadLambda_t> _loadLambdaTable;

	std::map<std::string, ComPtr<ID3D12Resource>> _resourceTable;
	std::map<std::wstring, ComPtr<ID3D12Resource>> _resourceTableW;

	void CreateTextureLoaderTable();

	ID3D12Resource* CreateTextureFromFile(const char* texpath);
	ID3D12Resource* CreateTextureFromFile(const std::wstring& texpath);

	ComPtr<ID3D12Resource> _whiteTex = nullptr;
	ComPtr<ID3D12Resource> _blackTex = nullptr;
	ComPtr<ID3D12Resource> _gradTex = nullptr;

	ID3D12Resource* CreateDefaultTexture(size_t width, size_t height);
	ID3D12Resource* CreateWhiteTexture();
	ID3D12Resource* CreateBlackTexture();
	ID3D12Resource* CreateGrayGradiationTexture();

	bool CreateAmbientOcclusionBuffer();
	bool CreateAmbientOcclusionDescriptorHeap();

	ComPtr<ID3D12DescriptorHeap> _heapForImgui;

	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeapForImgui();

	//Setting Value
	float _fov;
	float _lightVector[3];

public:
	Dx12Wrapper(HWND hwnd);
	~Dx12Wrapper();

	void Update();
	void Draw();
	void Clear();
	void BeginDraw();
	void EndDraw();
	void PreDrawShadow();
	bool PreDrawToPera1();
	void DrawToPera1();
	void PostDrawToPera1();
	void DrawBokeh();
	void DrawShrinkTextureForBlur();
	void SetCameraSetting();
	void DrawAmbientOcclusion();

	bool CreatePeraVertex();
	bool CreatePeraPipeline();

	ComPtr<ID3D12Resource> GetTextureByPath(const char* texpath);
	ComPtr<ID3D12Resource> GetTextureByPath(const std::wstring& texpath);

	ComPtr<ID3D12Resource> GetWhiteTexture();
	ComPtr<ID3D12Resource> GetBlackTexture();
	ComPtr<ID3D12Resource> GetGradTexture();

	ComPtr<ID3D12Device> Device();
	ComPtr<ID3D12GraphicsCommandList> CommandList();
	ComPtr<IDXGISwapChain4> Swapchain();

	ComPtr<ID3D12DescriptorHeap> GetHeapForImgui();

	void SetFov(float fov);

	void SetLightVector(float vec[3]);

	void SetScene();

	bool CheckResult(HRESULT& result, ID3DBlob* errBlob)
	{
		if (FAILED(result)) {
#ifdef _DEBUG
			if (errBlob != nullptr) {
				std::string outmsg;
				outmsg.resize(errBlob->GetBufferSize());
				std::copy_n(static_cast<char*>(errBlob->GetBufferPointer()),
					errBlob->GetBufferSize(),
					outmsg.begin());
				OutputDebugStringA(outmsg.c_str());//出力ウィンドウに出力してね
			}
			assert(SUCCEEDED(result));
#endif
			return false;
		}
		else {
			return true;
		}
	}
};

