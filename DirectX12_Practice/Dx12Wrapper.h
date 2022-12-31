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
	std::unique_ptr<D3D12_VIEWPORT> _viewport;
	std::unique_ptr<D3D12_RECT> _scissorrect;

	ComPtr<ID3D12Resource> _sceneConstBuff = nullptr;

	struct SceneMatricesData
	{
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX proj;
		XMFLOAT3 eye;
	};

	SceneMatricesData* _mappedSceneMatricesData;
	ComPtr<ID3D12DescriptorHeap> _sceneDescHeap = nullptr;

	ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _fenceVal = 0;

	HRESULT CreateFinalRenderTargets();

	HRESULT CreateDepthStencilView();

	HRESULT CreateSwapChain(const HWND& hwnd);

	HRESULT InitializeDXGIDevice();

	HRESULT InitializeCommand();

	HRESULT CreateSceneView();

	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map<std::string, LoadLambda_t> _loadLambdaTable;

	std::map<std::string, ComPtr<ID3D12Resource>> _resourceTable;

	void CreateTextureLoaderTable();

	ID3D12Resource* CreateTextureFromFile(const char* texpath);

public:
	Dx12Wrapper(HWND hwnd);
	~Dx12Wrapper();

	void Update();
	void BeginDraw();
	void EndDraw();

	ComPtr<ID3D12Resource> GetTextureByPath(const char* texpath);

	ComPtr<ID3D12Device> Device();
	ComPtr<ID3D12GraphicsCommandList> CommandList();
	ComPtr<IDXGISwapChain4> Swapchain();

	void SetScene();
};

