#pragma once
#include <memory>
#include <vector>

class Dx12Wrapper;
class PMXRenderer;
class PMXActor;
class FBXRenderer;
class FBXActor;
class InstancingRenderer;
class GeometryInstancingActor;
class IActor;

class Render
{
public:
	Render(std::shared_ptr<Dx12Wrapper>& dx);

	void Frame();
	void AddPMXActor(const std::shared_ptr<PMXActor>& actor);
	void AddFBXActor(const std::shared_ptr<FBXActor>& actor);
	void AddGeometryInstancingActor(const std::shared_ptr<GeometryInstancingActor>& actor) ;

private:
	void Update() const;
	void DrawStencil() const;
	void DrawPlanerReflection() const;
	void DrawShadowMap() const;
	void DrawOpaque() const;
	void PostProcess() const;
	void DrawFrame() const;
	void DrawImGui();
	void EndOfFrame() const;

private:
	std::shared_ptr<Dx12Wrapper> mDx12 = nullptr;
	std::shared_ptr<PMXRenderer> mPmxRenderer = nullptr;
	std::shared_ptr<FBXRenderer> mFbxRenderer = nullptr;
	std::shared_ptr<InstancingRenderer> mInstancingRenderer = nullptr;

	std::vector<std::shared_ptr<IActor>> mActorList;
};

