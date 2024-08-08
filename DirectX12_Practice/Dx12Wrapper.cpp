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
    mCurrentPPFlag(0)
{
#ifdef _DEBUG
	EnableDebugLayer();
#endif 

	auto& app = Application::Instance();
	mWindowSize = app.GetWindowSize();

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

	if (FAILED(mDevice->CreateFence(mFenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFence.ReleaseAndGetAddressOf()))))
	{
		assert(0);
		return;
	}

	mWhiteTex = CreateWhiteTexture();
	mBlackTex = CreateBlackTexture();
	mGradTex = CreateGrayGradationTexture();

	mHeapForImgui = CreateDescriptorHeapForImgui();
	if (mHeapForImgui == nullptr)
	{
		assert(0);
		return;
	}

	mCameraTransform = new Transform();
	mCameraTransform->SetPosition(0.0f, 10.0f, -30.0f);
	mCameraTransform->SetRotation(0.0f, 0.0f, 0.0f);

	mDirectionalLightTransform = new Transform();
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

		DirectX::XMFLOAT3 rotation = mCameraTransform->GetRotation();
		rotation.y = rotation.y + (senstive * deltaFloat * mouseX);
		rotation.x = rotation.x + (senstive * deltaFloat * mouseY);
		mCameraTransform->SetRotation(rotation.x, rotation.y, rotation.z);

		bool inputWASD = false;
		DirectX::XMVECTOR cameraForward = XMLoadFloat3(&mCameraTransform->GetForward());
		DirectX::XMVECTOR cameraRight = XMLoadFloat3(&mCameraTransform->GetRight());
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
			mCameraTransform->AddTranslation(moveVector.m128_f32[0], moveVector.m128_f32[1], moveVector.m128_f32[2]);
		}
	}

	auto wsize = Application::Instance().GetWindowSize();

	XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(mFov, static_cast<float>(wsize.cx) / static_cast<float>(wsize.cy), 0.1f, 1000.0f);

	XMVECTOR det;
	mMappedSceneMatricesData->view = mCameraTransform->GetViewMatrix();
	mMappedSceneMatricesData->proj = projectionMatrix;
	mMappedSceneMatricesData->invProj = XMMatrixInverse(&det, projectionMatrix);
	mMappedSceneMatricesData->eye = mCameraTransform->GetPosition();

	XMFLOAT4 planeVec(0, 1, 0, 0);
	XMFLOAT3 lightPosition = mDirectionalLightTransform->GetPosition();
	mMappedSceneMatricesData->shadow = XMMatrixShadow(XMLoadFloat4(&planeVec), -XMLoadFloat3(&lightPosition));

	XMFLOAT3 lightDirection = mDirectionalLightTransform->GetForward();
	mMappedSceneMatricesData->light = XMFLOAT4(lightDirection.x, lightDirection.y, lightDirection.z, 0);

	XMMATRIX lightMatrix = mDirectionalLightTransform->GetViewMatrix();
	mMappedSceneMatricesData->lightCamera = lightMatrix * XMMatrixOrthographicLH(40, 40, 1.0f, 100.0f);
}

void Dx12Wrapper::PreDrawStencil()
{
	auto handle = mDepthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	mCmdList->OMSetRenderTargets(0, nullptr, false, &handle);
	mCmdList->ClearDepthStencilView(handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mCmdList->OMSetStencilRef(1);
	
	auto wsize = Application::Instance().GetWindowSize();

	ID3D12DescriptorHeap* sceneheaps[] = { mSceneDescHeap.Get() };
	mCmdList->SetDescriptorHeaps(1, sceneheaps);
	mCmdList->SetGraphicsRootDescriptorTable(0, mSceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	mCmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	mCmdList->RSSetScissorRects(1, &rc);
}

void Dx12Wrapper::PreDrawReflection() const
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mReflectionBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	mCmdList->ResourceBarrier(1, &barrier);

	auto dsvHandle = mDepthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();
	dsvHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV) * 2;

	auto rtvHeapPointer = mPeraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHeapPointer.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * 6;

	mCmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, &dsvHandle);
	mCmdList->OMSetStencilRef(1);

	float clearColorBlack[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	mCmdList->ClearRenderTargetView(rtvHeapPointer, clearColorBlack, 0, nullptr);

	ID3D12DescriptorHeap* sceneheaps[] = { mSceneDescHeap.Get() };
	mCmdList->SetDescriptorHeaps(1, sceneheaps);

	auto heapHandle = mSceneDescHeap->GetGPUDescriptorHandleForHeapStart();
	mCmdList->SetGraphicsRootDescriptorTable(0, heapHandle);
}

void Dx12Wrapper::PreDrawShadow() const
{
	auto handle = mDepthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	mCmdList->OMSetRenderTargets(0, nullptr, false, &handle);

	mCmdList->ClearDepthStencilView(handle,
		D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	auto wsize = Application::Instance().GetWindowSize();

	ID3D12DescriptorHeap* heaps[] = { mSceneDescHeap.Get() };

	heaps[0] = mSceneDescHeap.Get();
	mCmdList->SetDescriptorHeaps(1, heaps);
	auto sceneHandle = mSceneDescHeap->GetGPUDescriptorHandleForHeapStart();
	mCmdList->SetGraphicsRootDescriptorTable(0, sceneHandle);

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, shadow_difinition, shadow_difinition);
	mCmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, shadow_difinition, shadow_difinition);
	mCmdList->RSSetScissorRects(1, &rc);
}

void Dx12Wrapper::PreDrawToPera1() const
{
	ChangeResourceBarrierFinalRenderTarget(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	ChangeToDepthWriteMainDepthBuffer();
	SetFinalRenderTarget();
	ClearFinalRenderTarget();
}

void Dx12Wrapper::DrawToPera1()
{
	auto wsize = Application::Instance().GetWindowSize();

	ID3D12DescriptorHeap* sceneheaps[] = { mSceneDescHeap.Get() };
	mCmdList->SetDescriptorHeaps(1, sceneheaps);
	mCmdList->SetGraphicsRootDescriptorTable(0, mSceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	mCmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	mCmdList->RSSetScissorRects(1, &rc);

	mCmdList->SetDescriptorHeaps(1, mDepthSRVHeap.GetAddressOf());
	auto handle = mDepthSRVHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mCmdList->SetGraphicsRootDescriptorTable(3, handle);
}

void Dx12Wrapper::DrawToPera1ForFbx()
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mReflectionBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	mCmdList->ResourceBarrier(1, &barrier);

	auto wsize = Application::Instance().GetWindowSize();

	ID3D12DescriptorHeap* sceneheaps[] = { mSceneDescHeap.Get() };
	mCmdList->SetDescriptorHeaps(1, sceneheaps);
	mCmdList->SetGraphicsRootDescriptorTable(0, mSceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	mCmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	mCmdList->RSSetScissorRects(1, &rc);

	ID3D12DescriptorHeap* renderTextureHeaps[] = { mPeraSRVHeap.Get() };
	mCmdList->SetDescriptorHeaps(1, renderTextureHeaps);

	auto reflectionTextureHandle = mPeraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	auto incSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	reflectionTextureHandle.ptr += incSize * 8;
	mCmdList->SetGraphicsRootDescriptorTable(3, reflectionTextureHandle);
}

void Dx12Wrapper::PostDrawToPera1()
{
	for (auto& resource : mPera1Resource)
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mCmdList->ResourceBarrier(1, &barrier);
	}
}

void Dx12Wrapper::DrawAmbientOcclusion()
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mAoBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCmdList->ResourceBarrier(1, &barrier);

	auto rtvBaseHandle = mAoRenderTargetViewDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	mCmdList->OMSetRenderTargets(1, &rtvBaseHandle, false, nullptr);
	mCmdList->SetGraphicsRootSignature(mPeraRootSignature.Get());

	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	mCmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	mCmdList->RSSetScissorRects(1, &rc);

	mCmdList->SetDescriptorHeaps(1, mPeraSRVHeap.GetAddressOf());

	auto srvHandle = mPeraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	mCmdList->SetGraphicsRootDescriptorTable(0, srvHandle);

	mCmdList->SetDescriptorHeaps(1, mDepthSRVHeap.GetAddressOf());

	auto srvDSVHandle = mDepthSRVHeap->GetGPUDescriptorHandleForHeapStart();
	mCmdList->SetGraphicsRootDescriptorTable(1, srvDSVHandle);

	mCmdList->SetDescriptorHeaps(1, mSceneDescHeap.GetAddressOf());

	auto sceneConstantViewHandle = mSceneDescHeap->GetGPUDescriptorHandleForHeapStart();
	mCmdList->SetGraphicsRootDescriptorTable(3, sceneConstantViewHandle);

	mCmdList->SetPipelineState(mAoPipeline.Get());
	mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	mCmdList->IASetVertexBuffers(0, 1, &mPeraVertexBufferView);
	mCmdList->DrawInstanced(4, 1, 0, 0);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(mAoBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mCmdList->ResourceBarrier(1, &barrier);
}

void Dx12Wrapper::DrawShrinkTextureForBlur()
{
	mCmdList->SetPipelineState(mBlurShrinkPipeline.Get());
	mCmdList->SetGraphicsRootSignature(mPeraRootSignature.Get());

	mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	mCmdList->IASetVertexBuffers(0, 1, &mPeraVertexBufferView);

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mBloomBuffer[0].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mCmdList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(mBloomBuffer[1].Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCmdList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(mDofBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCmdList->ResourceBarrier(1, &barrier);


	auto rtvBaseHandle = mPeraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	auto rtvIncSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = {};

	rtvHandles[0].InitOffsetted(rtvBaseHandle, rtvIncSize * 3);
	rtvHandles[1].InitOffsetted(rtvBaseHandle, rtvIncSize * 4);

	mCmdList->OMSetRenderTargets(2, rtvHandles, false, nullptr);

	auto srvHandle = mPeraSRVHeap->GetGPUDescriptorHandleForHeapStart();

	mCmdList->SetDescriptorHeaps(1, mPeraSRVHeap.GetAddressOf());
	mCmdList->SetGraphicsRootDescriptorTable(0, srvHandle);

	auto desc = mBloomBuffer[0]->GetDesc();
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
		mCmdList->RSSetViewports(1, &vp);
		mCmdList->RSSetScissorRects(1, &sr);
		mCmdList->DrawInstanced(4, 1, 0, 0);

		sr.top += vp.Height;
		vp.TopLeftX = 0;
		vp.TopLeftY = sr.top;

		vp.Width /= 2;
		vp.Height /= 2;
		sr.bottom = sr.top + vp.Height;
	}

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(mBloomBuffer[1].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mCmdList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(mDofBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mCmdList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(mBloomResultTexture.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCmdList->ResourceBarrier(1, &barrier);

	auto wsize = Application::Instance().GetWindowSize();

	vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	mCmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	mCmdList->RSSetScissorRects(1, &rc);

	mCmdList->SetPipelineState(mBlurResultPipeline.Get());
	mCmdList->SetGraphicsRootSignature(mBlurResultRootSignature.Get());

	mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	mCmdList->IASetVertexBuffers(0, 1, &mPeraVertexBufferView);

	rtvBaseHandle = mPeraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvBaseHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * 5;

	mCmdList->OMSetRenderTargets(1, &rtvBaseHandle, false, nullptr);

	srvHandle = mPeraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	srvHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2;

	mCmdList->SetDescriptorHeaps(1, mPeraSRVHeap.GetAddressOf());
	mCmdList->SetGraphicsRootDescriptorTable(0, srvHandle);

	auto bloomParameterSrvHandle = mPostProcessParameterSRVHeap->GetGPUDescriptorHandleForHeapStart();

	mCmdList->SetDescriptorHeaps(1, mPostProcessParameterSRVHeap.GetAddressOf());
	mCmdList->SetGraphicsRootDescriptorTable(1, bloomParameterSrvHandle);

	mCmdList->DrawInstanced(4, 1, 0, 0);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(mBloomResultTexture.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mCmdList->ResourceBarrier(1, &barrier);
}

void Dx12Wrapper::DrawScreenSpaceReflection()
{
	ChangeResourceBarrierFinalRenderTarget(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mSsrTexture.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCmdList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(mSsrMaskBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mCmdList->ResourceBarrier(1, &barrier);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(mReflectionBuffer.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mCmdList->ResourceBarrier(1, &barrier);

	ChangeToShaderResourceMainDepthBuffer();

	auto wsize = Application::Instance().GetWindowSize();

	auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	mCmdList->RSSetViewports(1, &viewport);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	mCmdList->RSSetScissorRects(1, &rc);

	mCmdList->SetPipelineState(mSsrPipeline.Get());
	mCmdList->SetGraphicsRootSignature(mSsrRootSignature.Get());

	mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	mCmdList->IASetVertexBuffers(0, 1, &mPeraVertexBufferView);

	auto rtvBaseHandle = mPeraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvBaseHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * 8;

	float clearColor[4] = { 0.0,0.0,0.0,1.0 };
	mCmdList->ClearRenderTargetView(rtvBaseHandle, clearColor, 0, nullptr);
	mCmdList->OMSetRenderTargets(1, &rtvBaseHandle, false, nullptr);

	mCmdList->SetDescriptorHeaps(1, mSceneDescHeap.GetAddressOf());
	mCmdList->SetGraphicsRootDescriptorTable(0, mSceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	mCmdList->SetDescriptorHeaps(1, mPeraSRVHeap.GetAddressOf());

	auto rtvSrvHandle = mPeraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	auto rtvSrvIncreaseSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mCmdList->SetGraphicsRootDescriptorTable(1, rtvSrvHandle);

	rtvSrvHandle.ptr += rtvSrvIncreaseSize;
	mCmdList->SetGraphicsRootDescriptorTable(2, rtvSrvHandle);

	auto ssrMaskHandle = mPeraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	ssrMaskHandle.ptr += rtvSrvIncreaseSize * 7;
	mCmdList->SetGraphicsRootDescriptorTable(3, ssrMaskHandle);

	auto planerReflectionHandle = mPeraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	planerReflectionHandle.ptr += rtvSrvIncreaseSize * 6;
	mCmdList->SetGraphicsRootDescriptorTable(4, planerReflectionHandle);

	mCmdList->SetDescriptorHeaps(1, mDepthSRVHeap.GetAddressOf());
	auto depthHandle = mDepthSRVHeap->GetGPUDescriptorHandleForHeapStart();
	mCmdList->SetGraphicsRootDescriptorTable(5, depthHandle);

	SetPostProcessParameterBuffer(6);
	SetResolutionDescriptorHeap(7);

	mCmdList->DrawInstanced(4, 1, 0, 0);

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(mSsrTexture.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mCmdList->ResourceBarrier(1, &barrier);

	ChangeToDepthWriteMainDepthBuffer();
	ChangeResourceBarrierFinalRenderTarget(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void Dx12Wrapper::Clear()
{
	auto backBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

	auto rtvHeapPointer = mRtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvHeapPointer.ptr += backBufferIndex * mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	mCmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, nullptr);

	float clsClr[4] = { 0.0,0.0,0.0,1.0 };
	mCmdList->ClearRenderTargetView(rtvHeapPointer, clsClr, 0, nullptr);
}

void Dx12Wrapper::Draw()
{
	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, wsize.cx, wsize.cy);
	mCmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	mCmdList->RSSetScissorRects(1, &rc);

	mCmdList->SetGraphicsRootSignature(mPeraRootSignature.Get());
	mCmdList->SetDescriptorHeaps(1, mPeraSRVHeap.GetAddressOf());

	auto handle = mPeraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	mCmdList->SetGraphicsRootDescriptorTable(0, handle);

	auto depthHandle = mDepthSRVHeap->GetGPUDescriptorHandleForHeapStart();
	mCmdList->SetDescriptorHeaps(1, mDepthSRVHeap.GetAddressOf());
	mCmdList->SetGraphicsRootDescriptorTable(1, depthHandle);

	mCmdList->SetDescriptorHeaps(1, mAoSRVDescriptorHeap.GetAddressOf());
	mCmdList->SetGraphicsRootDescriptorTable(2, mAoSRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	if ((mCurrentPPFlag & BLOOM) == BLOOM &&
		(mCurrentPPFlag & SSAO) == SSAO)
	{
		mCmdList->SetPipelineState(mScreenPipelineBloomSSAO.Get());
	}
	else if ((mCurrentPPFlag & BLOOM) == BLOOM)
	{
		mCmdList->SetPipelineState(mScreenPipelineBloom.Get());
	}
	else if ((mCurrentPPFlag & SSAO) == SSAO)
	{
		mCmdList->SetPipelineState(mScreenPipelineSSAO.Get());
	}
	else
	{
		mCmdList->SetPipelineState(mScreenPipelineDefault.Get());
	}

	mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	mCmdList->IASetVertexBuffers(0, 1, &mPeraVertexBufferView);
	//mCmdList->SetDescriptorHeaps(1, _distortionSRVHeap.GetAddressOf());
	//mCmdList->SetGraphicsRootDescriptorTable(2, _distortionSRVHeap->GetGPUDescriptorHandleForHeapStart());
	mCmdList->DrawInstanced(4, 1, 0, 0);
}

void Dx12Wrapper::Update()
{
	UpdateGlobalParameterBuffer();
}

void Dx12Wrapper::BeginDraw()
{
	int bbIdx = mSwapChain->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_BARRIER BarrierDesc = {};

	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.pResource = mBackBuffers[bbIdx];
	BarrierDesc.Transition.Subresource = 0;

	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	mCmdList->ResourceBarrier(1, &BarrierDesc);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvH = mRtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	auto dsvH = mDepthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();
	mCmdList->OMSetRenderTargets(1, &rtvH, true, &dsvH);
	mCmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	mCmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	mCmdList->RSSetViewports(1, mViewport.get());
	mCmdList->RSSetScissorRects(1, mScissorRect.get());
}

void Dx12Wrapper::EndDraw()
{
	int bbIdx = mSwapChain->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_BARRIER BarrierDesc = {};

	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.pResource = mBackBuffers[bbIdx];
	BarrierDesc.Transition.Subresource = 0;

	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	mCmdList->ResourceBarrier(1, &BarrierDesc);

	mCmdList->Close();

	ID3D12CommandList* cmdlists[] = { mCmdList.Get() };
	mCmdQueue->ExecuteCommandLists(1, cmdlists);

	mCmdQueue->Signal(mFence.Get(), ++mFenceVal);

	if (mFence->GetCompletedValue() != mFenceVal)
	{
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);

		mFence->SetEventOnCompletion(mFenceVal, event);

		WaitForSingleObject(event, INFINITE);

		CloseHandle(event);
	}

	mCmdAllocator->Reset();
	mCmdList->Reset(mCmdAllocator.Get(), nullptr);

	mSwapChain->Present(0, 0);
}

void Dx12Wrapper::SetRenderTargetByMainFrameBuffer() const
{
	auto rtvHeapPointer = mPeraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvHeapPointer = mDepthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();

	mCmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, &dsvHeapPointer);
}

void Dx12Wrapper::SetRenderTargetSSRMaskBuffer() const
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mSsrMaskBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	mCmdList->ResourceBarrier(1, &barrier);

	auto rtvHeapPointer = mPeraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHeapPointer.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * 7;

	auto dsvHeapPointer = mDepthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();

	mCmdList->OMSetRenderTargets(1, &rtvHeapPointer, false, &dsvHeapPointer);

	float clearColorBlack[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	mCmdList->ClearRenderTargetView(rtvHeapPointer, clearColorBlack, 0, nullptr);
}

void Dx12Wrapper::SetShaderResourceSSRMaskBuffer(unsigned int rootParameterIndex) const
{
	auto srvHandle = mPeraSRVHeap->GetGPUDescriptorHandleForHeapStart();
	srvHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 7;

	mCmdList->SetDescriptorHeaps(1, mPeraSRVHeap.GetAddressOf());
	mCmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, srvHandle);
}

void Dx12Wrapper::SetSceneBuffer(int rootParameterIndex) const
{
	ID3D12DescriptorHeap* sceneHeaps[] = { mSceneDescHeap.Get() };
	mCmdList->SetDescriptorHeaps(1, sceneHeaps);
	mCmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, mSceneDescHeap->GetGPUDescriptorHandleForHeapStart());
}

void Dx12Wrapper::SetRSSetViewportsAndScissorRectsByScreenSize() const
{
	const auto windowSize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, windowSize.cx, windowSize.cy);
	mCmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, windowSize.cx, windowSize.cy);
	mCmdList->RSSetScissorRects(1, &rc);
}

void Dx12Wrapper::SetLightDepthTexture(int rootParameterIndex) const
{
	mCmdList->SetDescriptorHeaps(1, mDepthSRVHeap.GetAddressOf());
	auto handle = mDepthSRVHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mCmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, handle);
}

void Dx12Wrapper::SetResolutionDescriptorHeap(unsigned rootParameterIndex) const
{
	ID3D12DescriptorHeap* heap[] = { mResolutionDescHeap.Get() };
	mCmdList->SetDescriptorHeaps(1, heap);
	mCmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, mResolutionDescHeap->GetGPUDescriptorHandleForHeapStart());
}

void Dx12Wrapper::SetGlobalParameterBuffer(unsigned rootParameterIndex) const
{
	mCmdList->SetGraphicsRootConstantBufferView(rootParameterIndex, mGlobalParameterBuffer->GetGPUVirtualAddress());
}

void Dx12Wrapper::SetPostProcessParameterBuffer(unsigned rootParameterIndex) const
{
	auto bloomParameterSrvHandle = mPostProcessParameterSRVHeap->GetGPUDescriptorHandleForHeapStart();
	mCmdList->SetDescriptorHeaps(1, mPostProcessParameterSRVHeap.GetAddressOf());
	mCmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, bloomParameterSrvHandle);
}

void Dx12Wrapper::ChangeResourceBarrierFinalRenderTarget(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) const
{
	for (auto& resource : mPera1Resource)
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), before, after);
		mCmdList->ResourceBarrier(1, &barrier);
	}

	for (auto& resource : mBloomBuffer)
	{
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), before, after);
		mCmdList->ResourceBarrier(1, &barrier);
	}
}

void Dx12Wrapper::SetFinalRenderTarget() const
{
	int rtvNum = 3;
	D3D12_CPU_DESCRIPTOR_HANDLE* rtvs = new D3D12_CPU_DESCRIPTOR_HANDLE[rtvNum];
	auto rtvHeapPointer = mPeraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	for (int i = 0; i < rtvNum; i++)
	{
		rtvs[i] = rtvHeapPointer;
		rtvHeapPointer.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	auto dsvHeapPointer = mDepthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();

	mCmdList->OMSetRenderTargets(3, rtvs, false, &dsvHeapPointer);
}

void Dx12Wrapper::SetOnlyDepthBuffer() const
{
	auto dsvHeapPointer = mDepthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();
	mCmdList->OMSetRenderTargets(0, nullptr, false, &dsvHeapPointer);
}

void Dx12Wrapper::ClearFinalRenderTarget() const
{
	int rtvNum = 3;
	D3D12_CPU_DESCRIPTOR_HANDLE* rtvs = new D3D12_CPU_DESCRIPTOR_HANDLE[rtvNum];
	auto rtvHeapPointer = mPeraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	for (int i = 0; i < rtvNum; i++)
	{
		rtvs[i] = rtvHeapPointer;
		rtvHeapPointer.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	auto dsvHeapPointer = mDepthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();

	float clearColorBlack[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	mCmdList->ClearRenderTargetView(rtvs[0], clearColorBlack, 0, nullptr);
	mCmdList->ClearRenderTargetView(rtvs[1], clearColorBlack, 0, nullptr);
	mCmdList->ClearRenderTargetView(rtvs[2], clearColorBlack, 0, nullptr);
	mCmdList->ClearDepthStencilView(dsvHeapPointer, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void Dx12Wrapper::ChangeToDepthWriteMainDepthBuffer() const
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthBuffer.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_DEPTH_WRITE);

	mCmdList->ResourceBarrier(1, &barrier);
}

void Dx12Wrapper::ChangeToShaderResourceMainDepthBuffer() const
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(mDepthBuffer.Get(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	mCmdList->ResourceBarrier(1, &barrier);
}

ComPtr<ID3D12Device> Dx12Wrapper::Device()
{
	return mDevice;
}

ComPtr<ID3D12GraphicsCommandList> Dx12Wrapper::CommandList()
{
	return mCmdList;
}

ComPtr<IDXGISwapChain4> Dx12Wrapper::SwapChain()
{
	return mSwapChain;
}

ComPtr<ID3D12DescriptorHeap> Dx12Wrapper::GetHeapForImgui()
{
	return mHeapForImgui;
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetTextureByPath(const char* texpath)
{
	auto it = mResourceTable.find(texpath);
	if (it != mResourceTable.end())
	{
		return mResourceTable[texpath];
	}
	else
	{
		return ComPtr<ID3D12Resource>(CreateTextureFromFile(texpath));
	}
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetTextureByPath(const std::wstring& texpath)
{
	auto it = mResourceTableW.find(texpath);
	if (it != mResourceTableW.end())
	{
		return mResourceTableW[texpath];
	}
	else
	{
		return ComPtr<ID3D12Resource>(CreateTextureFromFile(texpath));
	}
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetWhiteTexture()
{
	return mWhiteTex;
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetBlackTexture()
{
	return mBlackTex;
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetGradTexture()
{
	return mGradTex;
}

int Dx12Wrapper::GetBloomIteration() const
{
	return mMappedPostProcessParameter->iteration;
}

void Dx12Wrapper::SetBloomIteration(int iteration)
{
	if (mMappedPostProcessParameter == nullptr)
	{
		return;
	}

	mBloomIteration = iteration;
	mMappedPostProcessParameter->iteration = iteration;
}

float Dx12Wrapper::GetBloomIntensity() const
{
	return mMappedPostProcessParameter->intensity;
}

void Dx12Wrapper::SetBloomIntensity(float intensity)
{
	if (mMappedPostProcessParameter == nullptr)
	{
		return;
	}

	mMappedPostProcessParameter->intensity = intensity;
}

float Dx12Wrapper::GetScreenSpaceReflectionStepSize() const
{
	return mMappedPostProcessParameter->screenSpaceReflectionStepSize;
}

void Dx12Wrapper::SetScreenSpaceReflectionStepSize(float stepSize)
{
	if (mMappedPostProcessParameter == nullptr)
	{
		return;
	}

	mMappedPostProcessParameter->screenSpaceReflectionStepSize = stepSize;
}

void Dx12Wrapper::SetFov(float fov)
{
	mFov = fov;
}

void Dx12Wrapper::SetDirectionalLightRotation(float vec[3])
{
	mDirectionalLightTransform->SetRotation(vec[0], vec[1], vec[2]);
	DirectX::XMFLOAT3 storeVector = mDirectionalLightTransform->GetForward();
	DirectX::XMVECTOR lightDirection = DirectX::XMLoadFloat3(&storeVector);
	DirectX::XMVECTOR sceneCenter = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

	float shadowDistance = 50.0f;
	DirectX::XMVECTOR lightPosition = DirectX::XMVectorMultiplyAdd(lightDirection, XMVectorReplicate(-shadowDistance), sceneCenter);
	DirectX::XMStoreFloat3(&storeVector, lightPosition);

	mDirectionalLightTransform->SetPosition(storeVector.x, storeVector.y, storeVector.z);
}

const DirectX::XMFLOAT3& Dx12Wrapper::GetDirectionalLightRotation() const
{
	return mDirectionalLightTransform->GetRotation();
}

int Dx12Wrapper::GetPostProcessingFlag() const
{
	return mCurrentPPFlag;
}

void Dx12Wrapper::SetPostProcessingFlag(int flag)
{
	mCurrentPPFlag = flag;
}

DirectX::XMFLOAT3 Dx12Wrapper::GetCameraPosition() const
{
	return mCameraTransform->GetPosition();
}

DirectX::XMMATRIX Dx12Wrapper::GetViewMatrix() const
{
	return mMappedSceneMatricesData->view;
}

DirectX::XMMATRIX Dx12Wrapper::GetProjectionMatrix() const
{
	return mMappedSceneMatricesData->proj;
}

HRESULT Dx12Wrapper::InitializeDXGIDevice()
{
#ifdef _DEBUG
	HRESULT result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&mDXGIFactory));
#else
	HRESULT result = CreateDXGIFactory1(IID_PPV_ARGS(&mDXGIFactory));
#endif-
	if (FAILED(result))
	{
		return result;
	}

	std::vector<IDXGIAdapter*> adapters;

	IDXGIAdapter* tmpAdapter = nullptr;

	for (int i = 0; mDXGIFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
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
		if (D3D12CreateDevice(tmpAdapter, lv, IID_PPV_ARGS(mDevice.ReleaseAndGetAddressOf())) == S_OK)
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
	auto result = mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCmdAllocator.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(0);
		return result;
	}

	result = mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocator.Get(), nullptr, IID_PPV_ARGS(mCmdList.ReleaseAndGetAddressOf()));
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

	result = mDevice->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(mCmdQueue.ReleaseAndGetAddressOf()));
	assert(SUCCEEDED(result));
	return result;
}

HRESULT Dx12Wrapper::CreateSwapChain(const HWND& hwnd)
{
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};

	swapchainDesc.Width = mWindowSize.cx;
	swapchainDesc.Height = mWindowSize.cy;
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

	auto result = mDXGIFactory->CreateSwapChainForHwnd(mCmdQueue.Get(), hwnd, &swapchainDesc, nullptr, nullptr, (IDXGISwapChain1**)mSwapChain.ReleaseAndGetAddressOf());
	assert(SUCCEEDED(result));

	return result;
}

HRESULT Dx12Wrapper::CreateFinalRenderTargets()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = mSwapChain->GetDesc1(&desc);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	result = mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(mRtvHeaps.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		SUCCEEDED(result);
		return result;
	}

	DXGI_SWAP_CHAIN_DESC swcDesc = { };
	result = mSwapChain->GetDesc(&swcDesc);
	mBackBuffers.resize(swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = mRtvHeaps->GetCPUDescriptorHandleForHeapStart();

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (int idx = 0; idx < swcDesc.BufferCount; ++idx)
	{
		result = mSwapChain->GetBuffer(idx, IID_PPV_ARGS(&mBackBuffers[idx]));
		assert(SUCCEEDED(result));
		rtvDesc.Format = mBackBuffers[idx]->GetDesc().Format;
		mDevice->CreateRenderTargetView(mBackBuffers[idx], &rtvDesc, handle);
		handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	mViewport.reset(new CD3DX12_VIEWPORT(mBackBuffers[0]));
	mScissorRect.reset(new CD3DX12_RECT(0, 0, desc.Width, desc.Height));

	return result;
}

HRESULT Dx12Wrapper::CreateSceneView()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = mSwapChain->GetDesc1(&desc);
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneMatricesData) + 0xff) & ~0xff);

	result = mDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mSceneConstBuff.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	mMappedSceneMatricesData = nullptr;
	result = mSceneConstBuff->Map(0, nullptr, (void**)&mMappedSceneMatricesData);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	XMFLOAT3 eye(0, 10, -30);
	XMFLOAT3 target(0, 10, 0);
	XMFLOAT3 up(0, 1, 0);

	XMMATRIX lookMatrix = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(mFov, static_cast<float>(desc.Width) / static_cast<float>(desc.Height), 0.1f, 1000.0f);

	mMappedSceneMatricesData->view = lookMatrix;
	mMappedSceneMatricesData->proj = projectionMatrix;
	mMappedSceneMatricesData->eye = eye;

	XMFLOAT4 planeVec(0, 1, 0, 0);
	mMappedSceneMatricesData->shadow = XMMatrixShadow(XMLoadFloat4(&planeVec), -XMLoadFloat3(&eye));

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = mDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(mSceneDescHeap.ReleaseAndGetAddressOf()));

	auto heapHandle = mSceneDescHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = mSceneConstBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = mSceneConstBuff->GetDesc().Width;

	mDevice->CreateConstantBufferView(&cbvDesc, heapHandle);

	return result;
}

HRESULT Dx12Wrapper::CreateBloomParameterResource()
{
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	unsigned int size = sizeof(BloomParameter);
	unsigned int alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	unsigned int width = size + alignment - (size % alignment);

	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(width);

	auto result = mDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, 
		nullptr, 
		IID_PPV_ARGS(mPostProcessParameterBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result) == true)
	{
		return result;
	}

	result = mPostProcessParameterBuffer->Map(0, nullptr, (void**)&mMappedPostProcessParameter);
	if (FAILED(result) == true)
	{
		return result;
	}

	mMappedPostProcessParameter->iteration = 8;
	mMappedPostProcessParameter->intensity = 1.0f;
	mMappedPostProcessParameter->screenSpaceReflectionStepSize = 0.1f;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	result = mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(mPostProcessParameterSRVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result) == true)
	{
		return result;
	}

	auto handle = mPostProcessParameterSRVHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CONSTANT_BUFFER_VIEW_DESC bloomParamCbvDesc;
	bloomParamCbvDesc.BufferLocation = mPostProcessParameterBuffer->GetGPUVirtualAddress();
	bloomParamCbvDesc.SizeInBytes = mPostProcessParameterBuffer->GetDesc().Width;

	mDevice->CreateConstantBufferView(&bloomParamCbvDesc, handle);

	return result;
}

HRESULT Dx12Wrapper::CreateResolutionConstantBuffer()
{
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	unsigned int size = sizeof(ResolutionBuffer);
	unsigned int alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	unsigned int width = size + alignment - (size % alignment);

	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(width);

	auto result = mDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mResolutionConstBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result) == true)
	{
		return result;
	}

	result = mResolutionConstBuffer->Map(0, nullptr, (void**)&mMappedResolutionBuffer);
	if (FAILED(result) == true)
	{
		return result;
	}

	const SIZE windowSize = Application::Instance().GetWindowSize();
	mMappedResolutionBuffer->width = static_cast<float>(windowSize.cx);
	mMappedResolutionBuffer->height = static_cast<float>(windowSize.cy);

	mResolutionConstBuffer->Unmap(0, nullptr);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	result = mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(mResolutionDescHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result) == true)
	{
		return result;
	}

	auto handle = mResolutionDescHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc;
	constantBufferViewDesc.BufferLocation = mResolutionConstBuffer->GetGPUVirtualAddress();
	constantBufferViewDesc.SizeInBytes = mResolutionConstBuffer->GetDesc().Width;

	mDevice->CreateConstantBufferView(&constantBufferViewDesc, handle);

	return result;
}

HRESULT Dx12Wrapper::CreateGlobalParameterBuffer()
{
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	unsigned int size = sizeof(GlobalParameterBuffer);
	unsigned int alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	unsigned int width = size + alignment - (size % alignment);

	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(width);

	auto result = mDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mGlobalParameterBuffer.ReleaseAndGetAddressOf()));

	return result;
}

HRESULT Dx12Wrapper::CreatePeraResource()
{
	auto& bbuff = mBackBuffers[0];
	auto resDesc = bbuff->GetDesc();

	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	float clsClr[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
	D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clsClr);

	for (auto& resource : mPera1Resource)
	{
		auto result = mDevice->CreateCommittedResource(
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

	auto result = mDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValueBlack,
		IID_PPV_ARGS(mBloomResultTexture.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		return result;
	}

	result = mDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValueBlack,
		IID_PPV_ARGS(mReflectionBuffer.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		return result;
	}

	result = mDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValueBlack,
		IID_PPV_ARGS(mSsrMaskBuffer.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		return result;
	}

	result = mDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValueBlack,
		IID_PPV_ARGS(mSsrTexture.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		return result;
	}

	for (auto& resource : mBloomBuffer)
	{
		auto result = mDevice->CreateCommittedResource(
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

	result = mDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(mDofBuffer.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		return result;
	}

	auto heapDesc = mRtvHeaps->GetDesc();
	heapDesc.NumDescriptors = 9;
	result = mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(mPeraRTVHeap.ReleaseAndGetAddressOf()));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	auto handle = mPeraRTVHeap->GetCPUDescriptorHandleForHeapStart();

	for (auto& resource : mPera1Resource)
	{
		mDevice->CreateRenderTargetView(resource.Get(), &rtvDesc, handle);  //rtv offset 0, 1
		handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	for (auto& resource : mBloomBuffer)
	{
		mDevice->CreateRenderTargetView(resource.Get(), &rtvDesc, handle); //rtv offset 2, 3
		handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	mDevice->CreateRenderTargetView(mDofBuffer.Get(), &rtvDesc, handle); //rtv offset 4
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	mDevice->CreateRenderTargetView(mBloomResultTexture.Get(), &rtvDesc, handle); //rtv offset 5
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	mDevice->CreateRenderTargetView(mReflectionBuffer.Get(), &rtvDesc, handle); //rtv offset 6
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	mDevice->CreateRenderTargetView(mSsrMaskBuffer.Get(), &rtvDesc, handle); //rtv offset 7
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	mDevice->CreateRenderTargetView(mSsrTexture.Get(), &rtvDesc, handle); //rtv offset 8
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	heapDesc.NumDescriptors = 9;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;

	result = mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(mPeraSRVHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		return result;
	}

	handle = mPeraSRVHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = rtvDesc.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	for (auto& resource : mPera1Resource)
	{
		mDevice->CreateShaderResourceView(resource.Get(), &srvDesc, handle); //offset 0, 1
		handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); 
	}

	for (auto& resource : mBloomBuffer)
	{
		mDevice->CreateShaderResourceView(resource.Get(), &srvDesc, handle);  //offset 2, 3
		handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	mDevice->CreateShaderResourceView(mDofBuffer.Get(), &srvDesc, handle); //offset 4
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mDevice->CreateShaderResourceView(mBloomResultTexture.Get(), &srvDesc, handle); //offset 5
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mDevice->CreateShaderResourceView(mReflectionBuffer.Get(), &srvDesc, handle); //offset 6
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mDevice->CreateShaderResourceView(mSsrMaskBuffer.Get(), &srvDesc, handle); //offset 7
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mDevice->CreateShaderResourceView(mSsrTexture.Get(), &srvDesc, handle); //offset 8
	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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
	auto result = mDevice->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mPeraVB.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	PeraVertex* mappedPera = nullptr;
	mPeraVB->Map(0, nullptr, (void**)&mappedPera);
	copy(begin(pv), end(pv), mappedPera);
	mPeraVB->Unmap(0, nullptr);

	mPeraVertexBufferView.BufferLocation = mPeraVB->GetGPUVirtualAddress();
	mPeraVertexBufferView.SizeInBytes = sizeof(pv);
	mPeraVertexBufferView.StrideInBytes = sizeof(PeraVertex);
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

	result = mDevice->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(mPeraRootSignature.ReleaseAndGetAddressOf()));
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
	gpsDesc.pRootSignature = mPeraRootSignature.Get();

	result = D3DCompileFromFile(L"BlurPixel.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ps", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	result = mDevice->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(mBlurShrinkPipeline.ReleaseAndGetAddressOf()));

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
	result = mDevice->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(mScreenPipelineDefault.ReleaseAndGetAddressOf()));

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
	result = mDevice->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(mScreenPipelineBloom.ReleaseAndGetAddressOf()));

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
	result = mDevice->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(mScreenPipelineSSAO.ReleaseAndGetAddressOf()));

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
	result = mDevice->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(mScreenPipelineBloomSSAO.ReleaseAndGetAddressOf()));

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

	result = mDevice->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(mAoPipeline.ReleaseAndGetAddressOf()));

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

	result = mDevice->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(mBlurResultRootSignature.ReleaseAndGetAddressOf()));
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
	blurResultPipelineDesc.pRootSignature = mBlurResultRootSignature.Get();

	result = D3DCompileFromFile(L"BloomResult.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ps", "ps_5_0", 0, 0, ps.ReleaseAndGetAddressOf(), errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	blurResultPipelineDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	result = mDevice->CreateGraphicsPipelineState(&blurResultPipelineDesc, IID_PPV_ARGS(mBlurResultPipeline.ReleaseAndGetAddressOf()));
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

	//Resolution Buffer
	D3D12_DESCRIPTOR_RANGE resolutionBufferDescriptorRange = {};
	resolutionBufferDescriptorRange.NumDescriptors = 1;
	resolutionBufferDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	resolutionBufferDescriptorRange.BaseShaderRegister = 2;
	resolutionBufferDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	CD3DX12_ROOT_PARAMETER range[8] = {};

	range[0].InitAsDescriptorTable(1, &sceneBufferDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[1].InitAsDescriptorTable(1, &colorTexDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[2].InitAsDescriptorTable(1, &normalTexDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[3].InitAsDescriptorTable(1, &ssrMaskTexDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[4].InitAsDescriptorTable(1, &planerReflectionTexDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[5].InitAsDescriptorTable(1, &depthTexDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[6].InitAsDescriptorTable(1, &postProcessParameterDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	range[7].InitAsDescriptorTable(1, &resolutionBufferDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 8;
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

	result = mDevice->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(mSsrRootSignature.ReleaseAndGetAddressOf()));
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
	graphicsPipelineDesc.pRootSignature = mSsrRootSignature.Get();

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
	result = mDevice->CreateGraphicsPipelineState(&graphicsPipelineDesc, IID_PPV_ARGS(mSsrPipeline.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	return true;
}

bool Dx12Wrapper::CreateAmbientOcclusionBuffer()
{
	auto& bbuff = mBackBuffers[0];
	auto resDesc = bbuff->GetDesc();
	resDesc.Format = DXGI_FORMAT_R32_FLOAT;

	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Color[0] = clearValue.Color[1] = clearValue.Color[2] = clearValue.Color[3] = 1.0f;

	clearValue.Format = resDesc.Format;

	HRESULT result = S_OK;
	result = mDevice->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue, IID_PPV_ARGS(mAoBuffer.ReleaseAndGetAddressOf()));

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
	auto result = mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mAoRenderTargetViewDescriptorHeap.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(0);
		return false;
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	mDevice->CreateRenderTargetView(mAoBuffer.Get(), &rtvDesc, mAoRenderTargetViewDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;
	desc.NumDescriptors = 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mAoSRVDescriptorHeap.ReleaseAndGetAddressOf()));

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
	mDevice->CreateShaderResourceView(mAoBuffer.Get(), &srvDesc, mAoSRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	return true;
}

void Dx12Wrapper::CreateTextureLoaderTable()
{
	mLoadLambdaTable["sph"]
		= mLoadLambdaTable["spa"]
		= mLoadLambdaTable["bmp"]
		= mLoadLambdaTable["png"]
		= mLoadLambdaTable["jpg"]
		= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
		-> HRESULT
	{
		return LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, meta, img);
	};

	mLoadLambdaTable["tga"]
		= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
		-> HRESULT
	{
		return LoadFromTGAFile(path.c_str(), meta, img);
	};

	mLoadLambdaTable["dds"]
		= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
		-> HRESULT
	{
		return LoadFromDDSFile(path.c_str(), DDS_FLAGS_NONE, meta, img);
	};

}

HRESULT Dx12Wrapper::CreateDepthStencilView()
{
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    auto result = mSwapChain->GetDesc1(&desc);

	D3D12_RESOURCE_DESC depthResDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, desc.Width, desc.Height);
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;

	result = mDevice->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(mDepthBuffer.ReleaseAndGetAddressOf())
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

	result = mDevice->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&stencilResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&stencilClearValue,
		IID_PPV_ARGS(mStencilBuffer.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		return result;
	}

	depthResDesc.Width = shadow_difinition;
	depthResDesc.Height = shadow_difinition;
	result = mDevice->CreateCommittedResource(&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(mLightDepthBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		return result;
	}

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 3;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	result = mDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDepthStencilViewHeap.ReleaseAndGetAddressOf()));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	auto handle = mDepthStencilViewHeap->GetCPUDescriptorHandleForHeapStart();

	mDevice->CreateDepthStencilView(mDepthBuffer.Get() ,&dsvDesc, handle);

	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	mDevice->CreateDepthStencilView(mLightDepthBuffer.Get(), &dsvDesc, handle);

	handle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	mDevice->CreateDepthStencilView(mStencilBuffer.Get(), &dsvDesc, handle);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 3;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDepthSRVHeap));

	if (FAILED(result))
	{
		return result;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvResDesc = {};
	depthSrvResDesc.Format = DXGI_FORMAT_R32_FLOAT;
	depthSrvResDesc.Texture2D.MipLevels = 1;
	depthSrvResDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	depthSrvResDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

    auto srvHandle = mDepthSRVHeap->GetCPUDescriptorHandleForHeapStart();

	mDevice->CreateShaderResourceView(mDepthBuffer.Get(), &depthSrvResDesc, srvHandle);

	srvHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mDevice->CreateShaderResourceView(mLightDepthBuffer.Get(), &depthSrvResDesc, srvHandle);

	srvHandle.ptr += mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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

	auto result = mLoadLambdaTable[ext](
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
	result = mDevice->CreateCommittedResource(
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

	mResourceTable[texPath] = texbuff;
	return texbuff;
}

ID3D12Resource* Dx12Wrapper::CreateTextureFromFile(const std::wstring& texpath)
{
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	auto ext = GetExtension(texpath);

	auto result = mLoadLambdaTable[ext](
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
	result = mDevice->CreateCommittedResource(
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

	mResourceTableW[texpath] = texbuff;
	return texbuff;
}

ID3D12Resource* Dx12Wrapper::CreateDefaultTexture(size_t width, size_t height)
{
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height);
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	ID3D12Resource* buff = nullptr;
	auto result = mDevice->CreateCommittedResource(
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

	mDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(result.ReleaseAndGetAddressOf()));

	return result;
}

void Dx12Wrapper::UpdateGlobalParameterBuffer() const
{
	GlobalParameterBuffer* mapped;
	static std::random_device randomDevice;
	static std::mt19937 generator(randomDevice());
	static std::uniform_int_distribution<int> distribution(0, 99);
	
	mGlobalParameterBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	mapped->time = Time::GetTimeFloat();
	mapped->randomSeed = distribution(generator);
	mGlobalParameterBuffer->Unmap(0, nullptr);
}

