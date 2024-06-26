#include "PMDRenderer.h"
#include<d3dx12.h>
#include<cassert>
#include<d3dcompiler.h>
#include"Dx12Wrapper.h"
#include"PMDActor.h"
#include<string>
#include<algorithm>
#include "ImguiManager.h"

HRESULT PMDRenderer::CreateGraphicsPipelineForPMD()
{
	ComPtr<ID3DBlob> vsBlob = nullptr;
	ComPtr<ID3DBlob> psBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	UINT flags = 0;
#if defined( DEBUG ) || defined( _DEBUG )
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	auto result = D3DCompileFromFile(L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS",
		"vs_5_0",
		flags,
		0,
		&vsBlob,
		&errorBlob);

	if (!CheckShaderComplieResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	//result = D3DCompileFromFile(L"BasicPixelShader.hlsl",
	//	nullptr,
	//	D3D_COMPILE_STANDARD_FILE_INCLUDE,
	//	"BasicPS",
	//	"ps_5_0",
	//	flags,
	//	0,
	//	&psBlob,
	//	&errorBlob);

	//디퍼드 멀티렌더 타겟에 그릴때

	result = D3DCompileFromFile(L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"DeferrdPS",
		"ps_5_0",
		flags,
		0,
		&psBlob,
		&errorBlob);

	if (!CheckShaderComplieResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	//인풋 레이아웃
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
		},
		{
			"BONENO", 0, DXGI_FORMAT_R16G16_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"WEIGHT", 0, DXGI_FORMAT_R8_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"EDGE_FLG", 0, DXGI_FORMAT_R8_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

	gpipeline.pRootSignature = _rootSignature.Get();

	gpipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = psBlob->GetBufferSize();

	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;

	gpipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	gpipeline.NumRenderTargets = 3;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
	gpipeline.RTVFormats[1] = DXGI_FORMAT_B8G8R8A8_UNORM;
	gpipeline.RTVFormats[2] = DXGI_FORMAT_B8G8R8A8_UNORM;

	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;

	gpipeline.DepthStencilState.DepthEnable = true;
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	gpipeline.DepthStencilState.StencilEnable = false;
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	
	result = _dx12.Device()->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(_pipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
	}

    result = D3DCompileFromFile(L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ShadowVS",
		"vs_5_0",
		flags,
		0,
		&vsBlob,
		&errorBlob);

	if (!CheckShaderComplieResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
	gpipeline.PS.BytecodeLength = 0;
	gpipeline.PS.pShaderBytecode = nullptr;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	result = _dx12.Device()->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(_shadowPipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
	}

	return result;
}

HRESULT PMDRenderer::CreateRootSignature()
{
	//디스크립터 레인지 
	D3D12_DESCRIPTOR_RANGE descTblRange[5] = {};
	descTblRange[0].NumDescriptors = 1;
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[0].BaseShaderRegister = 0;
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descTblRange[1].NumDescriptors = 1;
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[1].BaseShaderRegister = 1;
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descTblRange[2].NumDescriptors = 1;
	descTblRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[2].BaseShaderRegister = 2;
	descTblRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descTblRange[3].NumDescriptors = 4;
	descTblRange[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[3].BaseShaderRegister = 0;
	descTblRange[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descTblRange[4] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);

	//루트 파라미터
	D3D12_ROOT_PARAMETER rootparam[4] = {};
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootparam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];
	rootparam[0].DescriptorTable.NumDescriptorRanges = 1;

	rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootparam[1].DescriptorTable.pDescriptorRanges = &descTblRange[1];
	rootparam[1].DescriptorTable.NumDescriptorRanges = 1;

	rootparam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootparam[2].DescriptorTable.pDescriptorRanges = &descTblRange[2];
	rootparam[2].DescriptorTable.NumDescriptorRanges = 2;

	rootparam[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootparam[3].DescriptorTable.pDescriptorRanges = &descTblRange[4];
	rootparam[3].DescriptorTable.NumDescriptorRanges = 1;

	//루트 시그니쳐
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};

	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootparam;
	rootSignatureDesc.NumParameters = 4;

	D3D12_STATIC_SAMPLER_DESC samplerDesc[3] = {};

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

	samplerDesc[1] = samplerDesc[0];
	samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[1].Filter = D3D12_FILTER_ANISOTROPIC;
	samplerDesc[1].ShaderRegister = 1;

	samplerDesc[2] = samplerDesc[0];
	samplerDesc[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc[2].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	samplerDesc[2].MaxAnisotropy = 1;
	samplerDesc[2].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	samplerDesc[2].ShaderRegister = 2;

	rootSignatureDesc.pStaticSamplers = samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 3;

    ComPtr<ID3DBlob> rootSigBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);

	if (FAILED(result) == true)
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = _dx12.Device()->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(_rootSignature.ReleaseAndGetAddressOf()));
	if (FAILED(result) == true)
	{
		assert(SUCCEEDED(result));
		return result;
	}

	return result;
}

bool PMDRenderer::CheckShaderComplieResult(HRESULT result, ID3DBlob* error)
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

PMDRenderer::PMDRenderer(Dx12Wrapper& dx12):
	_dx12(dx12)
{
	assert(SUCCEEDED(CreateRootSignature()));
	assert(SUCCEEDED(CreateGraphicsPipelineForPMD()));
}

PMDRenderer::~PMDRenderer()
{
}

void PMDRenderer::Update()
{
	for (auto& actor : _actors)
	{
		actor->Update();
	}
}

void PMDRenderer::PlayAnimation()
{
	for (auto& actor : _actors)
	{
		actor->PlayAnimation();
	}
}

void PMDRenderer::BeforeDrawFromLight()
{
	auto cmdList = _dx12.CommandList();
	cmdList->SetPipelineState(_shadowPipeline.Get());
	cmdList->SetGraphicsRootSignature(_rootSignature.Get());
}

void PMDRenderer::DrawFromLight()
{
	for (auto& actor : _actors)
	{
		actor->Draw(_dx12, true);
	}
}

void PMDRenderer::BeforeDraw()
{
	auto cmdList = _dx12.CommandList();
	cmdList->SetPipelineState(_pipeline.Get());
	cmdList->SetGraphicsRootSignature(_rootSignature.Get());
}

void PMDRenderer::Draw()
{
	for (auto& actor : _actors)
	{
		actor->Draw(_dx12, false);
	}
}

ID3D12PipelineState* PMDRenderer::GetPipelineState()
{
	return _pipeline.Get();
}

void PMDRenderer::AddActor(std::shared_ptr<PMDActor> actor)
{
	_actors.emplace_back(std::shared_ptr<PMDActor>(actor));
}

ID3D12PipelineState* PMDRenderer::GetShadowPipelineState()
{
	return _shadowPipeline.Get();
}

ID3D12RootSignature* PMDRenderer::GetRootSignature()
{
	return _rootSignature.Get();
}

Dx12Wrapper& PMDRenderer::GetDirect()
{
	return _dx12;
}
