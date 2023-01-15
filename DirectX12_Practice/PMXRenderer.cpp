#include "PMXRenderer.h"

HRESULT PMXRenderer::CreateGraphicsPipelineForPMX()
{
    return E_NOTIMPL;
}

HRESULT PMXRenderer::CreateRootSignature()
{
    return E_NOTIMPL;
}

PMXRenderer::PMXRenderer(Dx12Wrapper& dx12):
    _dx12(dx12)
{
}

PMXRenderer::~PMXRenderer()
{
}

void PMXRenderer::Update()
{
}

void PMXRenderer::Draw()
{
}

ID3D12PipelineState* PMXRenderer::GetPipelineState()
{
    return nullptr;
}

ID3D12RootSignature* PMXRenderer::GetRootSignature()
{
    return nullptr;
}
