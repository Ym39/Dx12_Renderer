#include "Render.h"

#include "Dx12Wrapper.h"
#include "PMXRenderer.h"
#include "PMXActor.h"
#include "FBXRenderer.h"
#include "FBXActor.h"
#include "InstancingRenderer.h"
#include "GeometryInstancingActor.h"
#include "ImguiManager.h"

Render::Render(std::shared_ptr<Dx12Wrapper>& dx):
mDx12(dx)
{
	mPmxRenderer.reset(new PMXRenderer(*mDx12));
	mFbxRenderer.reset(new FBXRenderer(*mDx12));
	mInstancingRenderer.reset(new InstancingRenderer(*mDx12));
}

void Render::Frame()
{
	Update();
	DrawStencil();
	DrawPlanerReflection();
	DrawShadowMap();
	DrawOpaque();
	PostProcess();
	DrawFrame();
	DrawImGui();
	EndOfFrame();
}

void Render::AddPMXActor(const std::shared_ptr<PMXActor>& actor) 
{
	mPmxRenderer->AddActor(actor);
	mActorList.push_back(actor);
}

void Render::AddFBXActor(const std::shared_ptr<FBXActor>& actor) 
{
	mFbxRenderer->AddActor(actor);
	mActorList.push_back(actor);
}

void Render::AddGeometryInstancingActor(const std::shared_ptr<GeometryInstancingActor>& actor) 
{
	mInstancingRenderer->AddActor(actor);
	mActorList.push_back(actor);
}

void Render::Update() const
{
	mDx12->Update();
	mPmxRenderer->Update();
	mFbxRenderer->Update();
	mInstancingRenderer->Update();
}

void Render::DrawStencil() const
{
	mFbxRenderer->BeforeWriteToStencil();
	mDx12->PreDrawStencil();
	mFbxRenderer->Draw();
}

void Render::DrawPlanerReflection() const
{
	mPmxRenderer->BeforeDrawReflection();
	mDx12->PreDrawReflection();
	mPmxRenderer->DrawReflection();
}

void Render::DrawShadowMap() const
{
	mPmxRenderer->BeforeDrawFromLight();
	mDx12->PreDrawShadow();
	mPmxRenderer->DrawFromLight();
}

void Render::DrawOpaque() const
{
	mDx12->PreDrawToPera1();

	mFbxRenderer->BeforeDrawAtForwardPipeline();
	mDx12->DrawToPera1ForFbx();
	mFbxRenderer->Draw();

	mPmxRenderer->BeforeDrawAtDeferredPipeline();
	mDx12->DrawToPera1();
	mPmxRenderer->Draw();

	mInstancingRenderer->BeforeDrawAtForwardPipeline();
	mInstancingRenderer->Draw();
}

void Render::PostProcess() const
{
	mDx12->PostDrawToPera1();
	mDx12->DrawAmbientOcclusion();
	mDx12->DrawShrinkTextureForBlur();
}

void Render::DrawFrame() const
{
	mDx12->Clear();
	mDx12->Draw();
}

void Render::DrawImGui()
{
	ImguiManager::Instance().StartUI();
	ImguiManager::Instance().UpdateAndSetDrawData(mDx12);
	ImguiManager::Instance().UpdatePostProcessMenu(mDx12, mPmxRenderer);
	ImguiManager::Instance().UpdateSaveMenu(mDx12, mFbxRenderer);
	ImguiManager::Instance().UpdateMaterialManagerWindow(mDx12);
	ImguiManager::Instance().UpdateActorManager(mDx12, mActorList);
	ImguiManager::Instance().EndUI(mDx12);
}

void Render::EndOfFrame() const
{
	mDx12->EndDraw();
	mInstancingRenderer->EndOfFrame();
}
