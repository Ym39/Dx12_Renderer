#include "FBXRenderer.h"
#include "Dx12Wrapper.h"
#include <d3dcompiler.h>
#include <d3dx12.h>

#include "FBXActor.h"

FBXRenderer::FBXRenderer(Dx12Wrapper& dx):
_dx(dx)
{
	auto result = CreateRootSignature();
	if (FAILED(result))
	{
		assert(0);
	}

	result = CreateGraphicsPipeline();
	if (FAILED(result))
	{
		assert(0);
	}
}

FBXRenderer::~FBXRenderer()
{
}

void FBXRenderer::Update()
{
	for (auto& actor : _actors)
	{
		actor->Update();
	}
}

void FBXRenderer::BeforeWriteToStencil()
{
	auto cmdList = _dx.CommandList();
	cmdList->SetPipelineState(_stencilWritePipeline.Get());
	cmdList->SetGraphicsRootSignature(_rootSignature.Get());
}

void FBXRenderer::BeforeDrawAtForwardPipeline()
{
	auto cmdList = _dx.CommandList();
	cmdList->SetPipelineState(_forwardPipeline.Get());
	cmdList->SetGraphicsRootSignature(_rootSignature.Get());
}

void FBXRenderer::Draw()
{
	_dx.SetResolutionDescriptorHeap(4);

	for (auto& actor : _actors)
	{
		actor->Draw(_dx, false);
	}
}

ID3D12PipelineState* FBXRenderer::GetPipelineState() const
{
	return _forwardPipeline.Get();
}

ID3D12RootSignature* FBXRenderer::GetRootSignature() const
{
	return _rootSignature.Get();
}

void FBXRenderer::AddActor(std::shared_ptr<FBXActor> actor)
{
	_actors.push_back(actor);
}

std::vector<std::shared_ptr<FBXActor>>& FBXRenderer::GetActor()
{
	return _actors;
}

HRESULT FBXRenderer::CreateRootSignature()
{
	D3D12_DESCRIPTOR_RANGE descriptorRange[5] = {};

	//Scene Buffer
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//World Transform Buffer
	descriptorRange[1].NumDescriptors = 1;
	descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descriptorRange[1].BaseShaderRegister = 1;
	descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//Material Buffer
	descriptorRange[2].NumDescriptors = 1;
	descriptorRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descriptorRange[2].BaseShaderRegister = 2;
	descriptorRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//Reflection Render Texture
	descriptorRange[3].NumDescriptors = 1;
	descriptorRange[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[3].BaseShaderRegister = 0;
	descriptorRange[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//Resolution Buffer
	descriptorRange[4].NumDescriptors = 1;
	descriptorRange[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descriptorRange[4].BaseShaderRegister = 3;
	descriptorRange[4].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	CD3DX12_ROOT_PARAMETER rootParam[5] = {};

	rootParam[0].InitAsDescriptorTable(1, &descriptorRange[0], D3D12_SHADER_VISIBILITY_ALL);
	rootParam[1].InitAsDescriptorTable(1, &descriptorRange[1], D3D12_SHADER_VISIBILITY_ALL);
	rootParam[2].InitAsDescriptorTable(1, &descriptorRange[2], D3D12_SHADER_VISIBILITY_ALL);
	rootParam[3].InitAsDescriptorTable(1, &descriptorRange[3], D3D12_SHADER_VISIBILITY_ALL);
	rootParam[4].InitAsDescriptorTable(1, &descriptorRange[4], D3D12_SHADER_VISIBILITY_ALL);

	CD3DX12_STATIC_SAMPLER_DESC samplerDesc[1] = {};
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc[0].Filter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR;
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc[0].MinLOD = 0.0f;
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc[0].ShaderRegister = 0;

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Init(_countof(rootParam), 
		rootParam, 
		_countof(samplerDesc),
		samplerDesc,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, &errorBlob);
	if (FAILED(result) == true)
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = _dx.Device()->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(_rootSignature.ReleaseAndGetAddressOf()));
	if (FAILED(result) == true)
	{
		assert(SUCCEEDED(result));
		return result;
	}

	return S_OK;
}

HRESULT FBXRenderer::CreateGraphicsPipeline()
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

	graphicsPipelineDesc.pRootSignature = _rootSignature.Get();
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

	auto result = D3DCompileFromFile(L"FBXVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"VS",
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

	result = D3DCompileFromFile(L"FBXPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PS",
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

	result = _dx.Device()->CreateGraphicsPipelineState(&graphicsPipelineDesc, IID_PPV_ARGS(_forwardPipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	graphicsPipelineDesc.NumRenderTargets = 0;
	graphicsPipelineDesc.PS = { nullptr, 0 };
	graphicsPipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	graphicsPipelineDesc.DepthStencilState.DepthEnable = false;
	graphicsPipelineDesc.DepthStencilState.StencilEnable = true;
	graphicsPipelineDesc.DepthStencilState.StencilReadMask = 0xFF;
	graphicsPipelineDesc.DepthStencilState.StencilWriteMask = 0xFF;

	graphicsPipelineDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	graphicsPipelineDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	graphicsPipelineDesc.RTVFormats[2] = DXGI_FORMAT_UNKNOWN;

	graphicsPipelineDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	graphicsPipelineDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	graphicsPipelineDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	graphicsPipelineDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	graphicsPipelineDesc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	graphicsPipelineDesc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	graphicsPipelineDesc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	graphicsPipelineDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	result = _dx.Device()->CreateGraphicsPipelineState(&graphicsPipelineDesc, IID_PPV_ARGS(_stencilWritePipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	return S_OK;
}

bool FBXRenderer::CheckShaderCompileResult(HRESULT result, ID3DBlob* error)
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
