#include "PMXRenderer.h"
#include<d3dx12.h>
#include<cassert>
#include<d3dcompiler.h>
#include"Dx12Wrapper.h"
#include "PMXActor.h"
#include<string>
#include<algorithm>

#include "ImguiManager.h"


HRESULT PMXRenderer::CreateGraphicsPipelineForPMX()
{
	ComPtr<ID3DBlob> vsBlob = nullptr;
	ComPtr<ID3DBlob> psBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	UINT flags = 0;
#if defined( DEBUG ) || defined( _DEBUG )
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	auto result = D3DCompileFromFile(L"PMXVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"VS",
		"vs_5_0",
		flags,
		0,
		&vsBlob,
		&errorBlob);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
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
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

	gpipeline.pRootSignature = _rootSignature.Get();

	gpipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = vsBlob->GetBufferSize();

	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	gpipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = true;

	D3D12_RENDER_TARGET_BLEND_DESC defaultBlendDesc;
	defaultBlendDesc.BlendEnable = false;
	defaultBlendDesc.LogicOpEnable = false;
	defaultBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	gpipeline.BlendState.RenderTarget[0] = transparencyBlendDesc;
	gpipeline.BlendState.RenderTarget[1] = defaultBlendDesc;
	gpipeline.BlendState.RenderTarget[2] = defaultBlendDesc;

	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	gpipeline.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;

	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;

	gpipeline.DepthStencilState.DepthEnable = true;
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	gpipeline.DepthStencilState.StencilEnable = false;
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	result = D3DCompileFromFile(L"PMXPixelShaderForward.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PS",
		"ps_5_0",
		flags,
		0,
		&psBlob,
		&errorBlob);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	gpipeline.NumRenderTargets = 1;
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
	result = _dx12.Device()->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(_forwardPipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
	}

	result = D3DCompileFromFile(L"PMXPixelShaderDeferred.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PS",
		"ps_5_0",
		flags,
		0,
		&psBlob,
		&errorBlob);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	gpipeline.RTVFormats[1] = DXGI_FORMAT_B8G8R8A8_UNORM;
	gpipeline.RTVFormats[2] = DXGI_FORMAT_B8G8R8A8_UNORM;

	gpipeline.NumRenderTargets = 3;
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());
	result = _dx12.Device()->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(_deferredPipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC reflectionPipelineDesc = {};

	reflectionPipelineDesc.pRootSignature = _rootSignature.Get();
	reflectionPipelineDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	reflectionPipelineDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
	reflectionPipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	reflectionPipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	reflectionPipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	reflectionPipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	reflectionPipelineDesc.BlendState.AlphaToCoverageEnable = false;
	reflectionPipelineDesc.BlendState.IndependentBlendEnable = true;
	reflectionPipelineDesc.InputLayout.pInputElementDescs = inputLayout;
	reflectionPipelineDesc.InputLayout.NumElements = _countof(inputLayout);
	reflectionPipelineDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	reflectionPipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	reflectionPipelineDesc.NumRenderTargets = 1;
	reflectionPipelineDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	reflectionPipelineDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
	reflectionPipelineDesc.SampleDesc.Count = 1;
	reflectionPipelineDesc.SampleDesc.Quality = 0;
	reflectionPipelineDesc.DepthStencilState.DepthEnable = true;
	reflectionPipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	reflectionPipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	reflectionPipelineDesc.DepthStencilState.StencilEnable = true;
	reflectionPipelineDesc.DepthStencilState.StencilReadMask = 0xFF;
	reflectionPipelineDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionPipelineDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionPipelineDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	reflectionPipelineDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	reflectionPipelineDesc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionPipelineDesc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionPipelineDesc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	reflectionPipelineDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	reflectionPipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	result = D3DCompileFromFile(L"PMXReflectionPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PS",
		"ps_5_0",
		flags,
		0,
		&psBlob,
		&errorBlob);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	reflectionPipelineDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());

	result = _dx12.Device()->CreateGraphicsPipelineState(&reflectionPipelineDesc, IID_PPV_ARGS(_reflectionPipeline.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
	}

	result = D3DCompileFromFile(L"PMXVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ShadowVS",
		"vs_5_0",
		flags,
		0,
		&vsBlob,
		&errorBlob);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
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

HRESULT PMXRenderer::CreateRootSignature()
{
	//디스크립터 레인지 
	D3D12_DESCRIPTOR_RANGE descTblRange[6] = {};
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

	descTblRange[3].NumDescriptors = 3;
	descTblRange[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[3].BaseShaderRegister = 0;
	descTblRange[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descTblRange[4] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);

	descTblRange[5] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 3);

	//루트 파라미터
	D3D12_ROOT_PARAMETER rootparam[5] = {};
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

	rootparam[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootparam[4].DescriptorTable.pDescriptorRanges = &descTblRange[5];
	rootparam[4].DescriptorTable.NumDescriptorRanges = 1;

	//루트 시그니쳐
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};

	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootparam;
	rootSignatureDesc.NumParameters = 5;

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

HRESULT PMXRenderer::CreateParameterBufferAndHeap()
{
	int parameterBufferSize = sizeof(ParameterBuffer);
	parameterBufferSize = (parameterBufferSize + 0xff) & ~0xff;

	auto result = _dx12.Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(parameterBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_parameterBuffer.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = _parameterBuffer->Map(0, nullptr, (void**)&_mappedParameterBuffer);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	_mappedParameterBuffer->bloomThreshold = 1.0f;

	D3D12_DESCRIPTOR_HEAP_DESC parameterDescHeapDesc = {};
	parameterDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	parameterDescHeapDesc.NodeMask = 0;
	parameterDescHeapDesc.NumDescriptors = 1;
	parameterDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = _dx12.Device()->CreateDescriptorHeap(&parameterDescHeapDesc, IID_PPV_ARGS(_parameterHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC parameterConstantBufferViewDesc = {};
	parameterConstantBufferViewDesc.BufferLocation = _parameterBuffer->GetGPUVirtualAddress();
	parameterConstantBufferViewDesc.SizeInBytes = parameterBufferSize;

	auto parameterDescHeapHandle = _parameterHeap->GetCPUDescriptorHandleForHeapStart();

	_dx12.Device()->CreateConstantBufferView(&parameterConstantBufferViewDesc, parameterDescHeapHandle);

	return S_OK;
}

bool PMXRenderer::CheckShaderCompileResult(HRESULT result, ID3DBlob* error)
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

PMXRenderer::PMXRenderer(Dx12Wrapper& dx12):
    _dx12(dx12)
{
	assert(SUCCEEDED(CreateRootSignature()));
	assert(SUCCEEDED(CreateGraphicsPipelineForPMX()));
	assert(SUCCEEDED(CreateParameterBufferAndHeap()));
}

PMXRenderer::~PMXRenderer()
{
}

void PMXRenderer::Update()
{
	for (auto& actor : _actors)
	{
		actor -> Update();
		actor-> UpdateAnimation();
	}
}

void PMXRenderer::BeforeDrawFromLight()
{
	auto cmdList = _dx12.CommandList();
	cmdList->SetPipelineState(_shadowPipeline.Get());
	cmdList->SetGraphicsRootSignature(_rootSignature.Get());
}

void PMXRenderer::BeforeDrawAtForwardPipeline()
{
	auto cmdList = _dx12.CommandList();
	cmdList->SetPipelineState(_forwardPipeline.Get());
	cmdList->SetGraphicsRootSignature(_rootSignature.Get());
}

void PMXRenderer::BeforeDrawAtDeferredPipeline()
{
	auto cmdList = _dx12.CommandList();
	cmdList->SetPipelineState(_deferredPipeline.Get());
	cmdList->SetGraphicsRootSignature(_rootSignature.Get());

	cmdList->SetDescriptorHeaps(1, _parameterHeap.GetAddressOf());
	cmdList->SetGraphicsRootDescriptorTable(4, _parameterHeap->GetGPUDescriptorHandleForHeapStart());
}

void PMXRenderer::BeforeDrawReflection()
{
	auto cmdList = _dx12.CommandList();
	cmdList->SetPipelineState(_reflectionPipeline.Get());
	cmdList->SetGraphicsRootSignature(_rootSignature.Get());
}

void PMXRenderer::DrawFromLight() const
{
	for (auto& actor : _actors)
	{
		actor->Draw(_dx12, true);
	}
}

void PMXRenderer::Draw() const
{
	for (auto& actor : _actors)
	{
		actor->Draw(_dx12, false);
	}
}

void PMXRenderer::DrawReflection() const
{
	for (auto& actor : _actors)
	{
		actor->DrawReflection(_dx12);
	}
}

void PMXRenderer::AddActor(std::shared_ptr<PMXActor> actor)
{
	_actors.push_back(actor);

	ImguiManager::Instance().SetPmxActor(actor.get());
}

const PMXActor* PMXRenderer::GetActor()
{
	return _actors[0].get();
}

void PMXRenderer::SetBloomThreshold(float threshold)
{
	if (_mappedParameterBuffer == nullptr)
	{
		return;
	}

	_mappedParameterBuffer->bloomThreshold = threshold;
}

float PMXRenderer::GetBloomThreshold() const
{
	if (_mappedParameterBuffer == nullptr)
	{
		return 1.0f;
	}

	return _mappedParameterBuffer->bloomThreshold;
}

ID3D12PipelineState* PMXRenderer::GetPipelineState()
{
    return _deferredPipeline.Get();
}

ID3D12RootSignature* PMXRenderer::GetRootSignature()
{
    return _rootSignature.Get();
}

Dx12Wrapper& PMXRenderer::GetDirect() const
{
	return _dx12;
}
