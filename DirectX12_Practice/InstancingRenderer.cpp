#include "InstancingRenderer.h"

#include <cassert>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include "Dx12Wrapper.h"
#include "GeometryInstancingActor.h"

InstancingRenderer::InstancingRenderer(Dx12Wrapper& dx):
mDirectX(dx)
{
	assert(SUCCEEDED(CreateRootSignature()));
	assert(SUCCEEDED(CreateGraphicsPipeline()));
}

InstancingRenderer::~InstancingRenderer()
{
}

void InstancingRenderer::Update()
{
	for (auto& actor : mActorList)
	{
		actor->Update();
	}
}

void InstancingRenderer::BeforeDrawAtForwardPipeline()
{
	auto cmdList = mDirectX.CommandList();
	cmdList->SetPipelineState(mForwardPipeline.Get());
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());
}

void InstancingRenderer::Draw()
{
	for (auto& actor : mActorList)
	{
		actor->Draw(mDirectX, false);
	}
}

void InstancingRenderer::EndOfFrame()
{
	for (auto& actor : mActorList)
	{
		actor->EndOfFrame(mDirectX);
	}
}

void InstancingRenderer::AddActor(std::shared_ptr<GeometryInstancingActor> actor)
{
	mActorList.push_back(actor);
}

HRESULT InstancingRenderer::CreateRootSignature()
{
	//Scene Buffer
	D3D12_DESCRIPTOR_RANGE sceneBufferDescriptorRange = {};
	sceneBufferDescriptorRange.NumDescriptors = 1;
	sceneBufferDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	sceneBufferDescriptorRange.BaseShaderRegister = 0;
	sceneBufferDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	CD3DX12_ROOT_PARAMETER rootParam[3] = {};

	rootParam[0].InitAsDescriptorTable(1, &sceneBufferDescriptorRange, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParam[1].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // SceneBuffer;
	rootParam[2].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_VERTEX); // GlobalParameterBuffer

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Init(_countof(rootParam), rootParam, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, &errorBlob);
	if (FAILED(result) == true)
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = mDirectX.Device()->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(mRootSignature.ReleaseAndGetAddressOf()));
	if (FAILED(result) == true)
	{
		assert(SUCCEEDED(result));
		return result;
	}

	return S_OK;
}

HRESULT InstancingRenderer::CreateGraphicsPipeline()
{
	UINT flags = 0;
#if defined( DEBUG ) || defined( _DEBUG )
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineDesc = {};

	graphicsPipelineDesc.pRootSignature = mRootSignature.Get();
	graphicsPipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	graphicsPipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	graphicsPipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	graphicsPipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	graphicsPipelineDesc.InputLayout.pInputElementDescs = inputLayout;
	graphicsPipelineDesc.InputLayout.NumElements = _countof(inputLayout);
	graphicsPipelineDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	graphicsPipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineDesc.SampleDesc.Count = 1;
	graphicsPipelineDesc.SampleDesc.Quality = 0;
	graphicsPipelineDesc.DepthStencilState.DepthEnable = true;
	graphicsPipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	graphicsPipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	graphicsPipelineDesc.DepthStencilState.StencilEnable = false;
	graphicsPipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	graphicsPipelineDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
	graphicsPipelineDesc.RTVFormats[1] = DXGI_FORMAT_B8G8R8A8_UNORM;
	graphicsPipelineDesc.RTVFormats[2] = DXGI_FORMAT_B8G8R8A8_UNORM;
	graphicsPipelineDesc.NumRenderTargets = 3;

	ComPtr<ID3DBlob> vs = nullptr;
	ComPtr<ID3DBlob> ps = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto result = D3DCompileFromFile(L"GeometryInstancingVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"vs_5_0",
		flags,
		0,
		&vs,
		&errorBlob);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	result = D3DCompileFromFile(L"GeometryInstancingPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"ps_5_0",
		flags,
		0,
		&ps,
		&errorBlob);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	graphicsPipelineDesc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	graphicsPipelineDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());

	result = mDirectX.Device()->CreateGraphicsPipelineState(&graphicsPipelineDesc, IID_PPV_ARGS(mForwardPipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	return S_OK;
}

bool InstancingRenderer::CheckShaderCompileResult(HRESULT result, ID3DBlob* error)
{
	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			::OutputDebugStringA("File Not Found");
		}
		else {
			std::string errstr;
			errstr.resize(error->GetBufferSize());
			std::copy_n((char*)error->GetBufferPointer(), error->GetBufferSize(), errstr.begin());
			errstr += "\n";
			OutputDebugStringA(errstr.c_str());
		}
		return false;
	}
	else {
		return true;
	}
}
