#include "Dx12Wrapper.h"
#include <cassert>
#include <d3dx12.h>
#include "Application.h"
#include "Utill.h"
#include "BitFlag.h"
#include "Input.h"
#include "Transform.h"
#include "Time.h"
#include <random>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

using namespace std;
using namespace DirectX;
using namespace Microsoft::WRL;

Dx12Wrapper::Dx12Wrapper(HWND hwnd) :
    _currentPPFlag(0)
{
#ifdef _DEBUG
	EnableDebugLayer();
#endif 

	auto& app = Application::Instance();
	_winSize = app.GetWindowSize();

	if (FAILED(InitializeDXGIDevice()))
	{
		assert(0);
		return;
	}

	if (FAILED(InitializeCommand()))
	{
		assert(0);
		return;
	}

	if (FAILED(CreateSwapChain(hwnd)))
	{
		assert(0);
		return;
	}

	if (FAILED(CreateFinalRenderTargets()))
	{
		assert(0);
		return;
	}

	if (FAILED(CreateSceneView()))
	{
		assert(0);
		return;
	}

	if (FAILED(CreateBloomParameterResource()))
	{
		assert(0);
		return;
	}


	if (FAILED(CreateResolutionConstantBuffer()))
	{
		assert(0);
		return;
	}

	if (FAILED(CreatePeraResource()))
	{
		assert(0);
		return;
	}

	if (FAILED(CreateGlobalParameterBuffer()))
	{
		assert(0);
		return;
	}

	if (CreatePeraVertex() == false)
	{
		assert(0);
		return;
	}

	if (CreatePeraPipeline() == false)
	{
		assert(0);
		return;
	}

	if (CreateSSRPipeline() == false)
	{
		assert(0);
		return;
	}

	if (CreateAmbientOcclusionBuffer() == false)
	{
		assert(0);
		return;
	}

	if (CreateAmbientOcclusionDescriptorHeap() == false)
	{
		assert(0);
		return;
	}

	CreateTextureLoaderTable();

	if (FAILED(CreateDepthStencilView()))
	{
		assert(0);
		return;
	}

	if (FAILED(_dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf()))))
	{
		assert(0);
		return;
	}

	_whiteTex = CreateWhiteTexture();
	_blackTex = CreateBlackTexture();
	_gradTex = CreateGrayGradationTexture();

	_heapForImgui = CreateDescriptorHeapForImgui();
	if (_heapForImgui == nullptr)
	{
		assert(0);
		return;
	}

	_cameraTransform = new Transform();
	_cameraTransform->SetPosition(0.0f, 10.0f, -30.0f);
	_cameraTransform->SetRotation(0.0f, 0.0f, 0.0f);

	_directionalLightTransform = new Transform();
}

Dx12Wrapper::~Dx12Wrapper()
{
}

void Dx12Wrapper::SetCameraSetting()
{
	if (Input::Instance()->GetMousePressed(1) == true)
	{
		float deltaFloat = Time::GetDeltaTime();
		float mouseX = Input::Instance()->GetMouseX();
		float mouseY = Input::Instance()->GetMouseY();
		
		float senstive = 0.025f;

		DirectX::XMFLOAT3 rotation = _cameraTransform->GetRotation();
		rotation.y = rotation.y + (senstive * deltaFloat * mouseX);
		rotation.x = rotation.x + (senstive * deltaFloat * mouseY);
		_cameraTransform->SetRotation(rotation.x, rotation.y, rotation.z);

		bool inputWASD = false;
		DirectX::XMVECTOR cameraForward = XMLoadFloat3(&_cameraTransform->GetForward());
		DirectX::XMVECTOR cameraRight = XMLoadFloat3(&_cameraTransform->GetRight());
		DirectX::XMVECTOR moveVector = XMVectorZero();

		if (Input::Instance()->GetKeyPressed(DIK_W) == true)
		{
			moveVector += cameraForward;
			inputWASD = true;
		}
		if (Input::Instance()->GetKeyPressed(DIK_S) == true)
		{
			moveVector -= cameraForward;
			inputWASD = true;
		}
		if (Input::Instance()->GetKeyPressed(DIK_A) == true)
		{
			moveVector -= cameraRight;
			inputWASD = true;
		}
		if (Input::Instance()->GetKeyPressed(DIK_D) == true)
		{
			moveVector += cameraRight;
			inputWASD = true;
		}

		if (inputWASD == true)
		{
			moveVector = XMVector3Normalize(moveVector);
			_cameraTransform->AddTranslation(moveVector.m128_f32[0], moveVector.m128_f32[1], moveVector.m128_f32[2]);
		}
	}

	auto wsize = Application::Instance().GetWindowSize();

	XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(_fov, static_cast<float>(wsize.cx) / static_cast<float>(wsize.cy), 0.1f, 1000.0f);

	XMVECTOR det;
	_mappedSceneMatricesData->view = _cameraTransform->GetViewMatrix();
	_mappedSceneMatricesData->proj = projectionMatrix;
	_mappedSceneMatricesData->invProj = XMMatrixInverse(&det, projectionMatrix);
	_mappedSceneMatricesData->eye = _cameraTransform->GetPosition();

	XMFLOAT4 planeVec(0, 1, 0, 0);
	XMFLOAT3 lightPosition = _directionalLightTransform->GetPosition();
	_mappedSceneMatricesData->shadow = XMMatrixShadow(XMLoadFloat4(&planeVec), -XMLoadFloat3(&lightPosition));

	XMFLOAT3 lightDirection = _directionalLightTransform->GetForward();
	_mappedSceneMatricesData->light = XMFLOAT4(lightDirection.x, lightDirection.y, lightDirection.z, 0);

	XMMATRIX lightMatrix = _directionalLightTransform->GetViewMatrix();
	_mappedSceneMatricesData->lightCamera = lightMatrix * XMMatrixOrthographicLH(40, 40, 1.0f, 100.0f);
}

void Dx12Wrapper::PreDrawStencil()
{
	auto handle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	_cmdList->OMSetRenderTargets(0, nullptr, false, &handle);
	_cmdList->ClearDepthStencilView(handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	_cmdList->OMSetStencilRef(1);
	
	auto wsize = Application::Instance().GetWindowSize();

	ID3D12DescriptorHeap* sceneheaps[] = { _sceneDescHeap.Get() };
	_cmdList->SetDescriptorHeaps(1, sceneheaps);
	_cmdList->SetGraphicsRootDescriptorTable(0, _sceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	_cmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	_cmdList->RSSetScissorRects(1, &rc);
}

void Dx12Wrapper::PreDrawReflection() const
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_reflectionBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	_cmdList->ResourceBarrier(1, &barrier);

	auto dsvHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	dsvHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV) * 2;

	auto rtvHeapPointer = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHeapPointer.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * 6;

	_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, &dsvHandle);
	_cmdList->OMSetStencilRef(1);

	float clearColorBlack[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	_cmdList->ClearRenderTargetView(rtvHeapPointer, clearColorBlack, 0, nullptr);

	ID3D12DescriptorHeap* sceneheaps[] = { _sceneDescHeap.Get() };
	_cmdList->SetDescriptorHeaps(1, sceneheaps);

	auto heapHandle = _sceneDescHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(0, heapHandle);
}

void Dx12Wrapper::PreDrawShadow() const
{
	auto handle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	_cmdList->OMSetRenderTargets(0, nullptr, false, &handle);

	_cmdList->ClearDepthStencilView(handle,
		D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	auto wsize = Application::Instance().GetWindowSize();

	ID3D12DescriptorHeap* heaps[] = { _sceneDescHeap.Get() };

	heaps[0] = _sceneDescHeap.Get();
	_cmdList->SetDescriptorHeaps(1, heaps);
	auto sceneHandle = _sceneDescHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(0, sceneHandle);

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, shadow_difinition, shadow_difinition);
	_cmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, shadow_difinition, shadow_difinition);
	_cmdList->RSSetScissorRects(1, &rc);
}

void Dx12Wrapper::PreDrawToPera1() const
{
	for (auto& resource : _pera1Resource)
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		_cmdList->ResourceBarrier(1, &barrier);
	}

	for (auto& resource : _bloomBuffer)
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		_cmdList->ResourceBarrier(1, &barrier);
	}

	ChangeToDepthWriteMainDepthBuffer();

	int rtvNum = 3;
	D3D12_CPU_DESCRIPTOR_HANDLE* rtvs = new D3D12_CPU_DESCRIPTOR_HANDLE[rtvNum];
	auto rtvHeapPointer = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	for (int i = 0; i < rtvNum; i++)
	{
		rtvs[i] = rtvHeapPointer;
		rtvHeapPointer.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	auto dsvHeapPointer = _dsvHeap->GetCPUDescriptorHandleForHeapStart();

	_cmdList->OMSetRenderTargets(3, rtvs, false, &dsvHeapPointer);

	float clearColorGray[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
	float clearColorBlack[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	_cmdList->ClearRenderTargetView(rtvs[0], clearColorBlack, 0, nullptr);
	_cmdList->ClearRenderTargetView(rtvs[1], clearColorBlack, 0, nullptr);
	_cmdList->ClearRenderTargetView(rtvs[2], clearColorBlack, 0, nullptr);

	_cmdList->ClearDepthStencilView(dsvHeapPointer, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void Dx12Wrapper::DrawToPera1()
{
	auto wsize = Application::Instance().GetWindowSize();

	ID3D12DescriptorHeap* sceneheaps[] = { _sceneDescHeap.Get() };
	_cmdList->SetDescriptorHeaps(1, sceneheaps);
	_cmdList->SetGraphicsRootDescriptorTable(0, _sceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	_cmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	_cmdList->RSSetScissorRects(1, &rc);

	_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
	auto handle = _depthSRVHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_cmdList->SetGraphicsRootDescriptorTable(3, handle);
}

void Dx12Wrapper::DrawToPera1ForFbx()
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_reflectionBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	_cmdList->ResourceBarrier(1, &barrier);

	auto wsize = Application::Instance().GetWindowSize();

	ID3D12DescriptorHeap* sceneheaps[] = { _sceneDescHeap.Get() };
	_cmdList->SetDescriptorHeaps(1, sceneheaps);
	_cmdList->SetGraphicsRootDescriptorTable(0, _sceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	_cmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	_cmdList->RSSetScissorRects(1, &rc);

	//Set Reflection RenderTexture;

	ID3D12DescriptorHeap* renderTextureHeaps[] = { _peraSRVHeap.Get() };
	_cmdList->SetDescriptorHeaps(1, renderTextureHeaps);

	auto reflectionTextureHandle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	auto incSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	reflectionTextureHandle.ptr += incSize * 6;
	_cmdList->SetGraphicsRootDescriptorTable(3, reflectionTextureHandle);
}

void Dx12Wrapper::PostDrawToPera1()
{
	for (auto& resource : _pera1Resource)
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		_cmdList->ResourceBarrier(1, &barrier);
	}
}

void Dx12Wrapper::DrawAmbientOcclusion()
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_aoBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	_cmdList->ResourceBarrier(1, &barrier);

	auto rtvBaseHandle = _aoRTVDH->GetCPUDescriptorHandleForHeapStart();
	_cmdList->OMSetRenderTargets(1, &rtvBaseHandle, false, nullptr);
	_cmdList->SetGraphicsRootSignature(_peraRS.Get());

	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	_cmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	_cmdList->RSSetScissorRects(1, &rc);

	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());

	auto srvHandle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(0, srvHandle);

	_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());

	auto srvDSVHandle = _depthSRVHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(1, srvDSVHandle);

	_cmdList->SetDescriptorHeaps(1, _sceneDescHeap.GetAddressOf());

	auto sceneConstantViewHandle = _sceneDescHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(3, sceneConstantViewHandle);

	_cmdList->SetPipelineState(_aoPipeline.Get());
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->DrawInstanced(4, 1, 0, 0);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(_aoBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(1, &barrier);
}

void Dx12Wrapper::DrawShrinkTextureForBlur()
{
	_cmdList->SetPipelineState(_blurShrinkPipeline.Get());
	_cmdList->SetGraphicsRootSignature(_peraRS.Get());

	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_bloomBuffer[0].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(_bloomBuffer[1].Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	_cmdList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(_dofBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	_cmdList->ResourceBarrier(1, &barrier);


	auto rtvBaseHandle = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	auto rtvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = {};

	rtvHandles[0].InitOffsetted(rtvBaseHandle, rtvIncSize * 3);
	rtvHandles[1].InitOffsetted(rtvBaseHandle, rtvIncSize * 4);

	_cmdList->OMSetRenderTargets(2, rtvHandles, false, nullptr);

	auto srvHandle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();

	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(0, srvHandle);

	auto desc = _bloomBuffer[0]->GetDesc();
	D3D12_VIEWPORT vp = {};
	D3D12_RECT sr = {};

	vp.MaxDepth = 1.0f;
	vp.MinDepth = 0.0f;
	vp.Height = desc.Height / 2;
	vp.Width = desc.Width / 2;
	sr.top = 0;
	sr.left = 0;
	sr.right = vp.Width;
	sr.bottom = vp.Height;

	for (int i = 0; i < mBloomIteration; ++i)
	{
		_cmdList->RSSetViewports(1, &vp);
		_cmdList->RSSetScissorRects(1, &sr);
		_cmdList->DrawInstanced(4, 1, 0, 0);

		sr.top += vp.Height;
		vp.TopLeftX = 0;
		vp.TopLeftY = sr.top;

		vp.Width /= 2;
		vp.Height /= 2;
		sr.bottom = sr.top + vp.Height;
	}

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(_bloomBuffer[1].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(_dofBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(_bloomResultTexture.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	_cmdList->ResourceBarrier(1, &barrier);

	auto wsize = Application::Instance().GetWindowSize();

	vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	_cmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	_cmdList->RSSetScissorRects(1, &rc);

	_cmdList->SetPipelineState(_blurResultPipeline.Get());
	_cmdList->SetGraphicsRootSignature(_blurResultRootSignature.Get());

	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);

	rtvBaseHandle = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvBaseHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * 5;

	_cmdList->OMSetRenderTargets(1, &rtvBaseHandle, false, nullptr);

	srvHandle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	srvHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2;

	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(0, srvHandle);

	auto bloomParameterSrvHandle = _postProcessParameterSRVHeap->GetGPUDescriptorHandleForHeapStart();

	_cmdList->SetDescriptorHeaps(1, _postProcessParameterSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(1, bloomParameterSrvHandle);

	_cmdList->DrawInstanced(4, 1, 0, 0);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(_bloomResultTexture.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(1, &barrier);
}

void Dx12Wrapper::DrawScreenSpaceReflection()
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_ssrTexture.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	_cmdList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(_ssrMaskBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(_reflectionBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(1, &barrier);

	ChangeToShaderResourceMainDepthBuffer();

	auto wsize = Application::Instance().GetWindowSize();

	auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	_cmdList->RSSetViewports(1, &viewport);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	_cmdList->RSSetScissorRects(1, &rc);

	_cmdList->SetPipelineState(_ssrPipeline.Get());
	_cmdList->SetGraphicsRootSignature(_ssrRootSignature.Get());

	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);

	auto rtvBaseHandle = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	auto rtvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandles = {};
	rtvHandles.InitOffsetted(rtvBaseHandle, rtvIncSize * 8);

	_cmdList->OMSetRenderTargets(1, &rtvHandles, false, nullptr);

	_cmdList->SetDescriptorHeaps(1, _sceneDescHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(0, _sceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());

	auto rtvSrvHandle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	auto rtvSrvIncreaseSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	_cmdList->SetGraphicsRootDescriptorTable(1, rtvSrvHandle);

	rtvSrvHandle.ptr += rtvSrvIncreaseSize;
	_cmdList->SetGraphicsRootDescriptorTable(2, rtvSrvHandle);

	auto ssrMaskHandle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	ssrMaskHandle.ptr += rtvSrvIncreaseSize * 7;
	_cmdList->SetGraphicsRootDescriptorTable(3, ssrMaskHandle);

	auto planerReflectionHandle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	planerReflectionHandle.ptr += rtvSrvIncreaseSize * 6;
	_cmdList->SetGraphicsRootDescriptorTable(4, planerReflectionHandle);

	_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
	auto depthHandle = _depthSRVHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(5, depthHandle);

	SetPostProcessParameterBuffer(6);

	_cmdList->DrawInstanced(4, 1, 0, 0);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(_ssrTexture.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(1, &barrier);
}

void Dx12Wrapper::Clear()
{
	auto bbIdx = _swapChain->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_BARRIER BarrierDesc = {};

	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
	BarrierDesc.Transition.Subresource = 0;

	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	auto rtvHeapPointer = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvHeapPointer.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, nullptr);

	float clsClr[4] = { 0.0,0.0,0.0,1.0 };
	_cmdList->ClearRenderTargetView(rtvHeapPointer, clsClr, 0, nullptr);
}

void Dx12Wrapper::Draw()
{
	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	_cmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	_cmdList->RSSetScissorRects(1, &rc);

	_cmdList->SetGraphicsRootSignature(_peraRS.Get());
	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());

	auto handle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetGraphicsRootDescriptorTable(0, handle);

	auto depthHandle = _depthSRVHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetDescriptorHeaps(1, _depthSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(1, depthHandle);

	_cmdList->SetDescriptorHeaps(1, _aoSRVDH.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(2, _aoSRVDH->GetGPUDescriptorHandleForHeapStart());

	if ((_currentPPFlag & BLOOM) == BLOOM &&
		(_currentPPFlag & SSAO) == SSAO)
	{
		_cmdList->SetPipelineState(_screenPipelineBloomSSAO.Get());
	}
	else if ((_currentPPFlag & BLOOM) == BLOOM)
	{
		_cmdList->SetPipelineState(_screenPipelineBloom.Get());
	}
	else if ((_currentPPFlag & SSAO) == SSAO)
	{
		_cmdList->SetPipelineState(_screenPipelineSSAO.Get());
	}
	else
	{
		_cmdList->SetPipelineState(_screenPipelineDefault.Get());
	}

	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	//_cmdList->SetDescriptorHeaps(1, _distortionSRVHeap.GetAddressOf());
	//_cmdList->SetGraphicsRootDescriptorTable(2, _distortionSRVHeap->GetGPUDescriptorHandleForHeapStart());
	_cmdList->DrawInstanced(4, 1, 0, 0);
}

void Dx12Wrapper::Update()
{
	UpdateGlobalParameterBuffer();
}

void Dx12Wrapper::BeginDraw()
{
	int bbIdx = _swapChain->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_BARRIER BarrierDesc = {};

	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
	BarrierDesc.Transition.Subresource = 0;

	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	_cmdList->ResourceBarrier(1, &BarrierDesc);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvH = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	auto dsvH = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	_cmdList->OMSetRenderTargets(1, &rtvH, true, &dsvH);
	_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	_cmdList->RSSetViewports(1, _viewport.get());
	_cmdList->RSSetScissorRects(1, _scissorrect.get());
}

void Dx12Wrapper::EndDraw()
{
	int bbIdx = _swapChain->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_BARRIER BarrierDesc = {};

	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
	BarrierDesc.Transition.Subresource = 0;

	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	_cmdList->ResourceBarrier(1, &BarrierDesc);

	_cmdList->Close();

	ID3D12CommandList* cmdlists[] = { _cmdList.Get() };
	_cmdQueue->ExecuteCommandLists(1, cmdlists);

	_cmdQueue->Signal(_fence.Get(), ++_fenceVal);

	if (_fence->GetCompletedValue() != _fenceVal)
	{
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);

		_fence->SetEventOnCompletion(_fenceVal, event);

		WaitForSingleObject(event, INFINITE);

		CloseHandle(event);
	}

	_cmdAllocator->Reset();
	_cmdList->Reset(_cmdAllocator.Get(), nullptr);

	_swapChain->Present(0, 0);
}

void Dx12Wrapper::SetRenderTargetByMainFrameBuffer() const
{
	auto rtvHeapPointer = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvHeapPointer = _dsvHeap->GetCPUDescriptorHandleForHeapStart();

	_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, &dsvHeapPointer);
}

void Dx12Wrapper::SetRenderTargetSSRMaskBuffer() const
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_ssrMaskBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	_cmdList->ResourceBarrier(1, &barrier);

	auto rtvHeapPointer = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHeapPointer.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * 7;

	auto dsvHeapPointer = _dsvHeap->GetCPUDescriptorHandleForHeapStart();

	_cmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, &dsvHeapPointer);

	float clearColorBlack[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	_cmdList->ClearRenderTargetView(rtvHeapPointer, clearColorBlack, 0, nullptr);
}

void Dx12Wrapper::SetShaderResourceSSRMaskBuffer(unsigned int rootParameterIndex) const
{
	auto srvHandle = _peraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	srvHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 7;

	_cmdList->SetDescriptorHeaps(1, _peraSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, srvHandle);
}

void Dx12Wrapper::SetSceneBuffer(int rootParameterIndex) const
{
	ID3D12DescriptorHeap* sceneHeaps[] = { _sceneDescHeap.Get() };
	_cmdList->SetDescriptorHeaps(1, sceneHeaps);
	_cmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, _sceneDescHeap->GetGPUDescriptorHandleForHeapStart());
}

void Dx12Wrapper::SetRSSetViewportsAndScissorRectsByScreenSize() const
{
	const auto windowSize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, windowSize.cx, windowSize.cy);
	_cmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, windowSize.cx, windowSize.cy);
	_cmdList->RSSetScissorRects(1, &rc);
}

void Dx12Wrapper::SetResolutionDescriptorHeap(unsigned rootParameterIndex) const
{
	ID3D12DescriptorHeap* heap[] = { _resolutionDescHeap.Get() };
	_cmdList->SetDescriptorHeaps(1, heap);
	_cmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, _resolutionDescHeap->GetGPUDescriptorHandleForHeapStart());
}

void Dx12Wrapper::SetGlobalParameterBuffer(unsigned rootParameterIndex) const
{
	_cmdList->SetGraphicsRootConstantBufferView(rootParameterIndex, _globalParameterBuffer->GetGPUVirtualAddress());
}

void Dx12Wrapper::SetPostProcessParameterBuffer(unsigned rootParameterIndex) const
{
	auto bloomParameterSrvHandle = _postProcessParameterSRVHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetDescriptorHeaps(1, _postProcessParameterSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, bloomParameterSrvHandle);
}

void Dx12Wrapper::ChangeToDepthWriteMainDepthBuffer() const
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_depthBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE);

	_cmdList->ResourceBarrier(1, &barrier);
}

void Dx12Wrapper::ChangeToShaderResourceMainDepthBuffer() const
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_depthBuffer.Get(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	_cmdList->ResourceBarrier(1, &barrier);
}

ComPtr<ID3D12Device> Dx12Wrapper::Device()
{
	return _dev;
}

ComPtr<ID3D12GraphicsCommandList> Dx12Wrapper::CommandList()
{
	return _cmdList;
}

ComPtr<IDXGISwapChain4> Dx12Wrapper::Swapchain()
{
	return _swapChain;
}

ComPtr<ID3D12DescriptorHeap> Dx12Wrapper::GetHeapForImgui()
{
	return _heapForImgui;
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetTextureByPath(const char* texpath)
{
	auto it = _resourceTable.find(texpath);
	if (it != _resourceTable.end())
	{
		return _resourceTable[texpath];
	}
	else
	{
		return ComPtr<ID3D12Resource>(CreateTextureFromFile(texpath));
	}
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetTextureByPath(const std::wstring& texpath)
{
	auto it = _resourceTableW.find(texpath);
	if (it != _resourceTableW.end())
	{
		return _resourceTableW[texpath];
	}
	else
	{
		return ComPtr<ID3D12Resource>(CreateTextureFromFile(texpath));
	}
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetWhiteTexture()
{
	return _whiteTex;
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetBlackTexture()
{
	return _blackTex;
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetGradTexture()
{
	return _gradTex;
}

int Dx12Wrapper::GetBloomIteration() const
{
	return _mappedPostProcessParameter->iteration;
}

void Dx12Wrapper::SetBloomIteration(int iteration)
{
	if (_mappedPostProcessParameter == nullptr)
	{
		return;
	}

	mBloomIteration = iteration;
	_mappedPostProcessParameter->iteration = iteration;
}

float Dx12Wrapper::GetBloomIntensity() const
{
	return _mappedPostProcessParameter->intensity;
}

void Dx12Wrapper::SetBloomIntensity(float intensity)
{
	if (_mappedPostProcessParameter == nullptr)
	{
		return;
	}

	_mappedPostProcessParameter->intensity = intensity;
}

float Dx12Wrapper::GetScreenSpaceReflectionStepSize() const
{
	return _mappedPostProcessParameter->screenSpaceReflectionStepSize;
}

void Dx12Wrapper::SetScreenSpaceReflectionStepSize(float stepSize)
{
	if (_mappedPostProcessParameter == nullptr)
	{
		return;
	}

	_mappedPostProcessParameter->screenSpaceReflectionStepSize = stepSize;
}

void Dx12Wrapper::SetFov(float fov)
{
	_fov = fov;
}

void Dx12Wrapper::SetDirectionalLightRotation(float vec[3])
{
	_directionalLightTransform->SetRotation(vec[0], vec[1], vec[2]);
	DirectX::XMFLOAT3 storeVector = _directionalLightTransform->GetForward();
	DirectX::XMVECTOR lightDirection = DirectX::XMLoadFloat3(&storeVector);
	DirectX::XMVECTOR sceneCenter = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

	float shadowDistance = 50.0f;
	DirectX::XMVECTOR lightPosition = DirectX::XMVectorMultiplyAdd(lightDirection, XMVectorReplicate(-shadowDistance), sceneCenter);
	DirectX::XMStoreFloat3(&storeVector, lightPosition);

	_directionalLightTransform->SetPosition(storeVector.x, storeVector.y, storeVector.z);
}

const DirectX::XMFLOAT3& Dx12Wrapper::GetDirectionalLightRotation() const
{
	return _directionalLightTransform->GetRotation();
}

int Dx12Wrapper::GetPostProcessingFlag() const
{
	return _currentPPFlag;
}

void Dx12Wrapper::SetPostProcessingFlag(int flag)
{
	_currentPPFlag = flag;
}

DirectX::XMFLOAT3 Dx12Wrapper::GetCameraPosition() const
{
	return _cameraTransform->GetPosition();
}

DirectX::XMMATRIX Dx12Wrapper::GetViewMatrix() const
{
	return _mappedSceneMatricesData->view;
}

DirectX::XMMATRIX Dx12Wrapper::GetProjectionMatrix() const
{
	return _mappedSceneMatricesData->proj;
}

HRESULT Dx12Wrapper::InitializeDXGIDevice()
{
#ifdef _DEBUG
	HRESULT result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
	HRESULT result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif-
	if (FAILED(result))
	{
		return result;
	}

	std::vector<IDXGIAdapter*> adapters;

	IDXGIAdapter* tmpAdapter = nullptr;

	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		if (strDesc.find(L"NVIDIA") != std::string::npos) {
			tmpAdapter = adpt;
			break;
		}
	}

	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	result = S_FALSE;

	D3D_FEATURE_LEVEL featureLevel;

	for (auto lv : levels)
	{
		if (D3D12CreateDevice(tmpAdapter, lv, IID_PPV_ARGS(_dev.ReleaseAndGetAddressOf())) == S_OK)
		{
			featureLevel = lv;
			result = S_OK;
			break;
		}
	}

	return result;
}

HRESULT Dx12Wrapper::InitializeCommand()
{
	auto result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_cmdAllocator.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(0);
		return result;
	}

	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator.Get(), nullptr, IID_PPV_ARGS(_cmdList.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(0);
		return result;
	}

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(_cmdQueue.ReleaseAndGetAddressOf()));
	assert(SUCCEEDED(result));
	return result;
}

HRESULT Dx12Wrapper::CreateSwapChain(const HWND& hwnd)
{
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};

	swapchainDesc.Width = _winSize.cx;
	swapchainDesc.Height = _winSize.cy;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	auto result = _dxgiFactory->CreateSwapChainForHwnd(_cmdQueue.Get(), hwnd, &swapchainDesc, nullptr, nullptr, (IDXGISwapChain1**)_swapChain.ReleaseAndGetAddressOf());
	assert(SUCCEEDED(result));

	return result;
}

HRESULT Dx12Wrapper::CreateFinalRenderTargets()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = _swapChain->GetDesc1(&desc);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_rtvHeaps.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		SUCCEEDED(result);
		return result;
	}

	DXGI_SWAP_CHAIN_DESC swcDesc = { };
	result = _swapChain->GetDesc(&swcDesc);
	_backBuffers.resize(swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (int idx = 0; idx < swcDesc.BufferCount; ++idx)
	{
		result = _swapChain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
		assert(SUCCEEDED(result));
		rtvDesc.Format = _backBuffers[idx]->GetDesc().Format;
		_dev->CreateRenderTargetView(_backBuffers[idx], &rtvDesc, handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	_viewport.reset(new CD3DX12_VIEWPORT(_backBuffers[0]));
	_scissorrect.reset(new CD3DX12_RECT(0, 0, desc.Width, desc.Height));

	return result;
}

HRESULT Dx12Wrapper::CreateSceneView()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = _swapChain->GetDesc1(&desc);
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneMatricesData) + 0xff) & ~0xff);

	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_sceneConstBuff.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	_mappedSceneMatricesData = nullptr;
	result = _sceneConstBuff->Map(0, nullptr, (void**)&_mappedSceneMatricesData);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	XMFLOAT3 eye(0, 10, -30);
	XMFLOAT3 target(0, 10, 0);
	XMFLOAT3 up(0, 1, 0);

	XMMATRIX lookMatrix = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(_fov, static_cast<float>(desc.Width) / static_cast<float>(desc.Height), 0.1f, 1000.0f);

	_mappedSceneMatricesData->view = lookMatrix;
	_mappedSceneMatricesData->proj = projectionMatrix;
	_mappedSceneMatricesData->eye = eye;

	XMFLOAT4 planeVec(0, 1, 0, 0);
	_mappedSceneMatricesData->shadow = XMMatrixShadow(XMLoadFloat4(&planeVec), -XMLoadFloat3(&eye));

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_sceneDescHeap.ReleaseAndGetAddressOf()));

	auto heapHandle = _sceneDescHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _sceneConstBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = _sceneConstBuff->GetDesc().Width;

	_dev->CreateConstantBufferView(&cbvDesc, heapHandle);

	return result;
}

HRESULT Dx12Wrapper::CreateBloomParameterResource()
{
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	unsigned int size = sizeof(BloomParameter);
	unsigned int alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	unsigned int width = size + alignment - (size % alignment);

	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(width);

	auto result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, 
		nullptr, 
		IID_PPV_ARGS(_postProcessParameterBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result) == true)
	{
		return result;
	}

	result = _postProcessParameterBuffer->Map(0, nullptr, (void**)&_mappedPostProcessParameter);
	if (FAILED(result) == true)
	{
		return result;
	}

	_mappedPostProcessParameter->iteration = 8;
	_mappedPostProcessParameter->intensity = 1.0f;
	_mappedPostProcessParameter->screenSpaceReflectionStepSize = 0.1f;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_postProcessParameterSRVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result) == true)
	{
		return result;
	}

	auto handle = _postProcessParameterSRVHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CONSTANT_BUFFER_VIEW_DESC bloomParamCbvDesc;
	bloomParamCbvDesc.BufferLocation = _postProcessParameterBuffer->GetGPUVirtualAddress();
	bloomParamCbvDesc.SizeInBytes = _postProcessParameterBuffer->GetDesc().Width;

	_dev->CreateConstantBufferView(&bloomParamCbvDesc, handle);

	return result;
}

HRESULT Dx12Wrapper::CreateResolutionConstantBuffer()
{
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	unsigned int size = sizeof(ResolutionBuffer);
	unsigned int alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	unsigned int width = size + alignment - (size % alignment);

	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(width);

	auto result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_resolutionConstBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result) == true)
	{
		return result;
	}

	result = _resolutionConstBuffer->Map(0, nullptr, (void**)&_mappedResolutionBuffer);
	if (FAILED(result) == true)
	{
		return result;
	}

	const SIZE windowSize = Application::Instance().GetWindowSize();
	_mappedResolutionBuffer->width = static_cast<float>(windowSize.cx);
	_mappedResolutionBuffer->height = static_cast<float>(windowSize.cy);

	_resolutionConstBuffer->Unmap(0, nullptr);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_resolutionDescHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result) == true)
	{
		return result;
	}

	auto handle = _resolutionDescHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc;
	constantBufferViewDesc.BufferLocation = _resolutionConstBuffer->GetGPUVirtualAddress();
	constantBufferViewDesc.SizeInBytes = _resolutionConstBuffer->GetDesc().Width;

	_dev->CreateConstantBufferView(&constantBufferViewDesc, handle);

	return result;
}

HRESULT Dx12Wrapper::CreateGlobalParameterBuffer()
{
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	unsigned int size = sizeof(GlobalParameterBuffer);
	unsigned int alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	unsigned int width = size + alignment - (size % alignment);

	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(width);

	auto result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_globalParameterBuffer.ReleaseAndGetAddressOf()));

	return result;
}

HRESULT Dx12Wrapper::CreatePeraResource()
{
	auto& bbuff = _backBuffers[0];
	auto resDesc = bbuff->GetDesc();

	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	float clsClr[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
	D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clsClr);

	for (auto& resource : _pera1Resource)
	{
		auto result = _dev->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())
		);

		if (FAILED(result))
		{
			return result;
		}
	}

	float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	D3D12_CLEAR_VALUE clearValueBlack = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, black);

	auto result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValueBlack,
		IID_PPV_ARGS(_bloomResultTexture.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		return result;
	}

	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValueBlack,
		IID_PPV_ARGS(_reflectionBuffer.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		return result;
	}

	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValueBlack,
		IID_PPV_ARGS(_ssrMaskBuffer.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		return result;
	}

	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValueBlack,
		IID_PPV_ARGS(_ssrTexture.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		return result;
	}

	for (auto& resource : _bloomBuffer)
	{
		auto result = _dev->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())
		);
		resDesc.Width >>= 1;

		if (FAILED(result))
		{
			return result;
		}
	}

	resDesc = bbuff->GetDesc();

	resDesc.Width >>= 1;

	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(_dofBuffer.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		return result;
	}

	auto heapDesc = _rtvHeaps->GetDesc();
	heapDesc.NumDescriptors = 9;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_peraRTVHeap.ReleaseAndGetAddressOf()));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	auto handle = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();

	for (auto& resource : _pera1Resource)
	{
		_dev->CreateRenderTargetView(resource.Get(), &rtvDesc, handle);  //rtv offset 0, 1
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	for (auto& resource : _bloomBuffer)
	{
		_dev->CreateRenderTargetView(resource.Get(), &rtvDesc, handle); //rtv offset 2, 3
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	_dev->CreateRenderTargetView(_dofBuffer.Get(), &rtvDesc, handle); //rtv offset 4
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_dev->CreateRenderTargetView(_bloomResultTexture.Get(), &rtvDesc, handle); //rtv offset 5
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_dev->CreateRenderTargetView(_reflectionBuffer.Get(), &rtvDesc, handle); //rtv offset 6
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_dev->CreateRenderTargetView(_ssrMaskBuffer.Get(), &rtvDesc, handle); //rtv offset 7
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_dev->CreateRenderTargetView(_ssrTexture.Get(), &rtvDesc, handle); //rtv offset 8
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	heapDesc.NumDescriptors = 9;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;

	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_peraSRVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		return result;
	}

	handle = _peraSRVHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = rtvDesc.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	for (auto& resource : _pera1Resource)
	{
		_dev->CreateShaderResourceView(resource.Get(), &srvDesc, handle); //offset 0, 1
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); 
	}

	for (auto& resource : _bloomBuffer)
	{
		_dev->CreateShaderResourceView(resource.Get(), &srvDesc, handle);  //offset 2, 3
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	_dev->CreateShaderResourceView(_dofBuffer.Get(), &srvDesc, handle); //offset 4
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	_dev->CreateShaderResourceView(_bloomResultTexture.Get(), &srvDesc, handle); //offset 5
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	_dev->CreateShaderResourceView(_reflectionBuffer.Get(), &srvDesc, handle); //offset 6
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	_dev->CreateShaderResourceView(_ssrMaskBuffer.Get(), &srvDesc, handle); //offset 7
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	_dev->CreateShaderResourceView(_ssrTexture.Get(), &srvDesc, handle); //offset 8
	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	return result;
}

bool Dx12Wrapper::CreatePeraVertex()
{
	struct PeraVertex
	{
		XMFLOAT3 pos;
		XMFLOAT2 uv;
	};

	PeraVertex pv[4] = { {{-1,-1,0.1},{0,1}},
						{{-1,1,0.1},{0,0}},
						{{1,-1,0.1},{1,1}},
						{{1,1,0.1},{1,0}} };

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(pv));
	auto result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_peraVB.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	PeraVertex* mappedPera = nullptr;
	_peraVB->Map(0, nullptr, (void**)&mappedPera);
	copy(begin(pv), end(pv), mappedPera);
	_peraVB->Unmap(0, nullptr);

	_peraVBV.BufferLocation = _peraVB->GetGPUVirtualAddress();
	_peraVBV.SizeInBytes = sizeof(pv);
	_peraVBV.StrideInBytes = sizeof(PeraVertex);
	return true;
}

bool Dx12Wrapper::CreatePeraPipeline()
{
	D3D12_DESCRIPTOR_RANGE range[4] = {};

	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[0].BaseShaderRegister = 0;
	range[0].NumDescriptors = 6;

	range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[1].BaseShaderRegister = 6;
	range[1].NumDescriptors = 1;

	range[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[2].BaseShaderRegister = 7;
	range[2].NumDescriptors = 1;

	range[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	range[3].BaseShaderRegister = 0;
	range[3].NumDescriptors = 1;

	D3D12_ROOT_PARAMETER rp[4] = {};

	rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[0].DescriptorTable.pDescriptorRanges = &range[0];
	rp[0].DescriptorTable.NumDescriptorRanges = 1;

	rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[1].DescriptorTable.pDescriptorRanges = &range[1];
	rp[1].DescriptorTable.NumDescriptorRanges = 1;

	rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[2].DescriptorTable.pDescriptorRanges = &range[2];
	rp[2].DescriptorTable.NumDescriptorRanges = 1;

	rp[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rp[3].DescriptorTable.pDescriptorRanges = &range[3];
	rp[3].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 4;
	rsDesc.pParameters = rp;

	D3D12_STATIC_SAMPLER_DESC sampler = CD3DX12_STATIC_SAMPLER_DESC(0);
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	rsDesc.pStaticSamplers = &sampler;
	rsDesc.NumStaticSamplers = 1;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


	ComPtr<ID3DBlob> rsBlob;
	ComPtr<ID3DBlob> errBlob;

	auto result = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, rsBlob.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	result = _dev->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(_peraRS.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	ComPtr<ID3DBlob> vs;
	ComPtr<ID3DBlob> ps;

	result = D3DCompileFromFile(L"ScreenVertex.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"vs", "vs_5_0", 0, 0, vs.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = {};
	gpsDesc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	gpsDesc.DepthStencilState.DepthEnable = false;
	gpsDesc.DepthStencilState.StencilEnable = false;

	D3D12_INPUT_ELEMENT_DESC layout[2] =
	{
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};

	gpsDesc.InputLayout.NumElements = _countof(layout);
	gpsDesc.InputLayout.pInputElementDescs = layout;
	gpsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpsDesc.NumRenderTargets = 2;
	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpsDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpsDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpsDesc.SampleDesc.Count = 1;
	gpsDesc.SampleDesc.Quality = 0;
	gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	gpsDesc.pRootSignature = _peraRS.Get();

	result = D3DCompileFromFile(L"BlurPixel.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ps", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_blurShrinkPipeline.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	gpsDesc.NumRenderTargets = 1;
	gpsDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;

	result = D3DCompileFromFile(L"ScreenPixelForward.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ps", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_screenPipelineDefault.ReleaseAndGetAddressOf()));

	D3D_SHADER_MACRO enableBloomDefines[] =
	{
		"BLOOM", "1", NULL, NULL
	};

	result = D3DCompileFromFile(L"ScreenPixelForward.hlsl",
		enableBloomDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ps", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_screenPipelineBloom.ReleaseAndGetAddressOf()));

	D3D_SHADER_MACRO enableSSAODefines[] =
	{
		"SSAO", "1", NULL, NULL
	};

	result = D3DCompileFromFile(L"ScreenPixelForward.hlsl",
		enableSSAODefines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ps", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_screenPipelineSSAO.ReleaseAndGetAddressOf()));

	D3D_SHADER_MACRO enableBloomSSAODefines[] =
	{
		"SSAO", "1", "BLOOM", "1", NULL, NULL
	};

	result = D3DCompileFromFile(L"ScreenPixelForward.hlsl",
		enableBloomSSAODefines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ps", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_screenPipelineBloomSSAO.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(0);
		return false;
	}


	UINT flags = 0;
#if defined( DEBUG ) || defined( _DEBUG )
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	result = D3DCompileFromFile(L"SSAO.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"SsaoPs", "ps_5_0",
		flags, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	gpsDesc.NumRenderTargets = 1;
	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
	gpsDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	gpsDesc.BlendState.RenderTarget[0].BlendEnable = false;
	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());

	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_aoPipeline.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(0);
		return false;
	}


	D3D12_DESCRIPTOR_RANGE blurResultRange[2] = {};
	blurResultRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	blurResultRange[0].BaseShaderRegister = 0;
	blurResultRange[0].NumDescriptors = 2;

	blurResultRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	blurResultRange[1].BaseShaderRegister = 0;
	blurResultRange[1].NumDescriptors = 1;

	D3D12_ROOT_PARAMETER blurResultRootParameter[2] = {};
	blurResultRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	blurResultRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	blurResultRootParameter[0].DescriptorTable.pDescriptorRanges = &blurResultRange[0];
	blurResultRootParameter[0].DescriptorTable.NumDescriptorRanges = 1;

	blurResultRootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	blurResultRootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	blurResultRootParameter[1].DescriptorTable.pDescriptorRanges = &blurResultRange[1];
	blurResultRootParameter[1].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC blurResultRootSignatureDesc = {};
	blurResultRootSignatureDesc.NumParameters = 2;
	blurResultRootSignatureDesc.pParameters = blurResultRootParameter;
	blurResultRootSignatureDesc.pStaticSamplers = &sampler;
	blurResultRootSignatureDesc.NumStaticSamplers = 1;
	blurResultRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	result = D3D12SerializeRootSignature(&blurResultRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, rsBlob.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	result = _dev->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(_blurResultRootSignature.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC blurResultPipelineDesc = {};
	blurResultPipelineDesc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	blurResultPipelineDesc.DepthStencilState.DepthEnable = false;
	blurResultPipelineDesc.DepthStencilState.StencilEnable = false;
	blurResultPipelineDesc.InputLayout.NumElements = _countof(layout);
	blurResultPipelineDesc.InputLayout.pInputElementDescs = layout;
	blurResultPipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	blurResultPipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	blurResultPipelineDesc.NumRenderTargets = 1;
	blurResultPipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	blurResultPipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	blurResultPipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	blurResultPipelineDesc.SampleDesc.Count = 1;
	blurResultPipelineDesc.SampleDesc.Quality = 0;
	blurResultPipelineDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	blurResultPipelineDesc.pRootSignature = _blurResultRootSignature.Get();

	result = D3DCompileFromFile(L"BloomResult.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ps", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	blurResultPipelineDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	result = _dev->CreateGraphicsPipelineState(&blurResultPipelineDesc, IID_PPV_ARGS(_blurResultPipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	return true;
}

bool Dx12Wrapper::CreateSSRPipeline()
{
	UINT flags = 0;
#if defined( DEBUG ) || defined( _DEBUG )
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	//Scene Buffer
	D3D12_DESCRIPTOR_RANGE sceneBufferDescriptorRange = {};
	sceneBufferDescriptorRange.NumDescriptors = 1;
	sceneBufferDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	sceneBufferDescriptorRange.BaseShaderRegister = 0;
	sceneBufferDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//Color 
	D3D12_DESCRIPTOR_RANGE colorTexDescriptorRange = {};
	colorTexDescriptorRange.NumDescriptors = 1;
	colorTexDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	colorTexDescriptorRange.BaseShaderRegister = 0;
	colorTexDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//Normal
	D3D12_DESCRIPTOR_RANGE normalTexDescriptorRange = {};
	normalTexDescriptorRange.NumDescriptors = 1;
	normalTexDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	normalTexDescriptorRange.BaseShaderRegister = 1;
	normalTexDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//SSR Mask
	D3D12_DESCRIPTOR_RANGE ssrMaskTexDescriptorRange = {};
	ssrMaskTexDescriptorRange.NumDescriptors = 1;
	ssrMaskTexDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ssrMaskTexDescriptorRange.BaseShaderRegister = 2;
	ssrMaskTexDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//Planer Reflection
	D3D12_DESCRIPTOR_RANGE planerReflectionTexDescriptorRange = {};
	planerReflectionTexDescriptorRange.NumDescriptors = 1;
	planerReflectionTexDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	planerReflectionTexDescriptorRange.BaseShaderRegister = 3;
	planerReflectionTexDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//Depth
	D3D12_DESCRIPTOR_RANGE depthTexDescriptorRange = {};
	depthTexDescriptorRange.NumDescriptors = 1;
	depthTexDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	depthTexDescriptorRange.BaseShaderRegister = 4;
	depthTexDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//Post Process Parameter
	D3D12_DESCRIPTOR_RANGE postProcessParameterDescriptorRange = {};
	postProcessParameterDescriptorRange.NumDescriptors = 1;
	postProcessParameterDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	postProcessParameterDescriptorRange.BaseShaderRegister = 1;
	postProcessParameterDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	CD3DX12_ROOT_PARAMETER range[7] = {};

	range[0].InitAsDescriptorTable(1, &sceneBufferDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[1].InitAsDescriptorTable(1, &colorTexDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[2].InitAsDescriptorTable(1, &normalTexDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[3].InitAsDescriptorTable(1, &ssrMaskTexDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[4].InitAsDescriptorTable(1, &planerReflectionTexDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[5].InitAsDescriptorTable(1, &depthTexDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[6].InitAsDescriptorTable(1, &postProcessParameterDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 7;
	rsDesc.pParameters = range;

	D3D12_STATIC_SAMPLER_DESC sampler = CD3DX12_STATIC_SAMPLER_DESC(0);
	sampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 1;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	sampler.MinLOD = 0;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	rsDesc.pStaticSamplers = &sampler;
	rsDesc.NumStaticSamplers = 1;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> rsBlob;
	ComPtr<ID3DBlob> errBlob;

	auto result = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, rsBlob.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	result = _dev->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(_ssrRootSignature.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	ComPtr<ID3DBlob> vs;
	ComPtr<ID3DBlob> ps;

	D3D12_INPUT_ELEMENT_DESC layout[2] =
	{
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D12_APPEND_ALIGNED_ELEMENT,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineDesc = {};
	graphicsPipelineDesc.DepthStencilState.DepthEnable = false;
	graphicsPipelineDesc.DepthStencilState.StencilEnable = false;
	graphicsPipelineDesc.InputLayout.NumElements = _countof(layout);
	graphicsPipelineDesc.InputLayout.pInputElementDescs = layout;
	graphicsPipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	graphicsPipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineDesc.NumRenderTargets = 1;
	graphicsPipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	graphicsPipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	graphicsPipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	graphicsPipelineDesc.SampleDesc.Count = 1;
	graphicsPipelineDesc.SampleDesc.Quality = 0;
	graphicsPipelineDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	graphicsPipelineDesc.pRootSignature = _ssrRootSignature.Get();

	result = D3DCompileFromFile(L"SSRVertexShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", "vs_5_0", flags, 0, vs.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	result = D3DCompileFromFile(L"SSRPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", "ps_5_0", flags, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	graphicsPipelineDesc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	graphicsPipelineDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	result = _dev->CreateGraphicsPipelineState(&graphicsPipelineDesc, IID_PPV_ARGS(_ssrPipeline.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	return true;
}

bool Dx12Wrapper::CreateAmbientOcclusionBuffer()
{
	auto& bbuff = _backBuffers[0];
	auto resDesc = bbuff->GetDesc();
	resDesc.Format = DXGI_FORMAT_R32_FLOAT;

	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Color[0] = clearValue.Color[1] = clearValue.Color[2] = clearValue.Color[3] = 1.0f;

	clearValue.Format = resDesc.Format;

	HRESULT result = S_OK;
	result = _dev->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue, IID_PPV_ARGS(_aoBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result) == true)
	{
		assert(0);
		return false;
	}

	return true;
}

bool Dx12Wrapper::CreateAmbientOcclusionDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;
	desc.NumDescriptors = 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	auto result = _dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_aoRTVDH.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	_dev->CreateRenderTargetView(_aoBuffer.Get(), &rtvDesc, _aoRTVDH->GetCPUDescriptorHandleForHeapStart());

	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;
	desc.NumDescriptors = 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = _dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(_aoSRVDH.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	_dev->CreateShaderResourceView(_aoBuffer.Get(), &srvDesc, _aoSRVDH->GetCPUDescriptorHandleForHeapStart());

	return true;
}

void Dx12Wrapper::CreateTextureLoaderTable()
{
	_loadLambdaTable["sph"]
		= _loadLambdaTable["spa"]
		= _loadLambdaTable["bmp"]
		= _loadLambdaTable["png"]
		= _loadLambdaTable["jpg"]
		= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
		-> HRESULT
	{
		return LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, meta, img);
	};

	_loadLambdaTable["tga"]
		= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
		-> HRESULT
	{
		return LoadFromTGAFile(path.c_str(), meta, img);
	};

	_loadLambdaTable["dds"]
		= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
		-> HRESULT
	{
		return LoadFromDDSFile(path.c_str(), DDS_FLAGS_NONE, meta, img);
	};

}

HRESULT Dx12Wrapper::CreateDepthStencilView()
{
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    auto result = _swapChain->GetDesc1(&desc);

	D3D12_RESOURCE_DESC depthResDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, desc.Width, desc.Height);
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;

	result = _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(_depthBuffer.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		return result;
	}

	D3D12_RESOURCE_DESC stencilResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D24_UNORM_S8_UINT, desc.Width, desc.Height);
	stencilResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE stencilClearValue = {};
	stencilClearValue.DepthStencil.Stencil = 0.0f;
	stencilClearValue.DepthStencil.Depth = 1.0f;
	stencilClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	result = _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&stencilResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&stencilClearValue,
		IID_PPV_ARGS(_stencilBuffer.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		return result;
	}

	depthResDesc.Width = shadow_difinition;
	depthResDesc.Height = shadow_difinition;
	result = _dev->CreateCommittedResource(&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(_lightDepthBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		return result;
	}

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 3;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	result = _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_dsvHeap.ReleaseAndGetAddressOf()));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	auto handle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();

	_dev->CreateDepthStencilView(_depthBuffer.Get() ,&dsvDesc, handle);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	_dev->CreateDepthStencilView(_lightDepthBuffer.Get(), &dsvDesc, handle);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	_dev->CreateDepthStencilView(_stencilBuffer.Get(), &dsvDesc, handle);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 3;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_depthSRVHeap));

	if (FAILED(result))
	{
		return result;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvResDesc = {};
	depthSrvResDesc.Format = DXGI_FORMAT_R32_FLOAT;
	depthSrvResDesc.Texture2D.MipLevels = 1;
	depthSrvResDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	depthSrvResDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

    auto srvHandle = _depthSRVHeap->GetCPUDescriptorHandleForHeapStart();

	_dev->CreateShaderResourceView(_depthBuffer.Get(), &depthSrvResDesc, srvHandle);

	srvHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	_dev->CreateShaderResourceView(_lightDepthBuffer.Get(), &depthSrvResDesc, srvHandle);

	srvHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    return result;
}

ID3D12Resource* Dx12Wrapper::CreateWhiteTexture()
{
	ID3D12Resource* whiteBuff = CreateDefaultTexture(4, 4);

	std::vector<unsigned char> data(4 * 4 * 4);
	std::fill(data.begin(), data.end(), 0xff);

	auto result = whiteBuff->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		4 * 4,
		data.size()
	);

	assert(SUCCEEDED(result));

	return whiteBuff;
}

ID3D12Resource* Dx12Wrapper::CreateBlackTexture()
{
	ID3D12Resource* blackBuff = CreateDefaultTexture(4, 4);

	std::vector<unsigned char> data(4 * 4 * 4);
	std::fill(data.begin(), data.end(), 0x00);

	auto result = blackBuff->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		4 * 4,
		data.size()
	);
	assert(SUCCEEDED(result));

	return blackBuff;
}

ID3D12Resource* Dx12Wrapper::CreateGrayGradationTexture()
{
	ID3D12Resource* gradBuff = CreateDefaultTexture(4, 4);

	std::vector<unsigned char> data(4 * 256);
	auto it = data.begin();
	unsigned int c = 0xff;
	for (; it != data.end(); it += 4)
	{
		auto col = (0xff << 24) | RGB(c, c, c);
		std::fill(it, it + 4, col);
		--c;
	}

	auto result = gradBuff->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		4 * sizeof(unsigned int),
		sizeof(unsigned int) * data.size()
	);

	assert(SUCCEEDED(result));

	return gradBuff;
}

ID3D12Resource* Dx12Wrapper::CreateTextureFromFile(const char* texpath)
{
	string texPath = texpath;

	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	auto wtexpath = GetWideStringFromString(texPath);

	auto ext = GetExtension(texPath);

	auto result = _loadLambdaTable[ext](
		wtexpath,
		&metadata,
		scratchImg
		);

	if (FAILED(result))
	{
		return nullptr;
	}

	auto img = scratchImg.GetImage(0, 0, 0);

	D3D12_HEAP_PROPERTIES texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata.format,
		metadata.width,
		metadata.height,
		metadata.arraySize,
		metadata.mipLevels
	);

	ID3D12Resource* texbuff = nullptr;
	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);

	if (FAILED(result))
	{
		return nullptr;
	}

	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		img->pixels,
		img->rowPitch,
		img->slicePitch
	);

	if (FAILED(result))
	{
		return nullptr;
	}

	_resourceTable[texPath] = texbuff;
	return texbuff;
}

ID3D12Resource* Dx12Wrapper::CreateTextureFromFile(const std::wstring& texpath)
{
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	auto ext = GetExtension(texpath);

	auto result = _loadLambdaTable[ext](
		texpath,
		&metadata,
		scratchImg
		);

	if (FAILED(result))
	{
		return nullptr;
	}

	auto img = scratchImg.GetImage(0, 0, 0);

	D3D12_HEAP_PROPERTIES texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata.format,
		metadata.width,
		metadata.height,
		metadata.arraySize,
		metadata.mipLevels
	);

	ID3D12Resource* texbuff = nullptr;
	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);

	if (FAILED(result))
	{
		return nullptr;
	}

	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		img->pixels,
		img->rowPitch,
		img->slicePitch
	);

	if (FAILED(result))
	{
		return nullptr;
	}

	_resourceTableW[texpath] = texbuff;
	return texbuff;
}

ID3D12Resource* Dx12Wrapper::CreateDefaultTexture(size_t width, size_t height)
{
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height);
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	ID3D12Resource* buff = nullptr;
	auto result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&buff)
	);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return nullptr;
	}
	return buff;
}

ComPtr<ID3D12DescriptorHeap> Dx12Wrapper::CreateDescriptorHeapForImgui()
{
	ComPtr<ID3D12DescriptorHeap> result;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;
	desc.NumDescriptors = 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	_dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(result.ReleaseAndGetAddressOf()));

	return result;
}

void Dx12Wrapper::UpdateGlobalParameterBuffer() const
{
	GlobalParameterBuffer* mapped;
	static std::random_device randomDevice;
	static std::mt19937 generator(randomDevice());
	static std::uniform_int_distribution<int> distribution(0, 99);
	
	_globalParameterBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	mapped->time = Time::GetTimeFloat();
	mapped->randomSeed = distribution(generator);
	_globalParameterBuffer->Unmap(0, nullptr);
}

