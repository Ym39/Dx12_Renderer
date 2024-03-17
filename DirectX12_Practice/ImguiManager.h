#pragma once
#include<d3d12.h>
#include<Windows.h>
#include<memory>
#include<format>
#include <vector>

class PMXActor;
class PMXRenderer;
struct LoadMaterial;

constexpr float pi = 3.141592653589f;

class Dx12Wrapper;

struct IMGUI_RESULT_OUPUT
{
	float Fov;
	float LightVector[3];
};

class ImguiManager
{
public:
	static ImguiManager& Instance();

	bool Initialize(HWND hwnd, std::shared_ptr<Dx12Wrapper> dx);
	void StartUI();
	void EndUI(std::shared_ptr<Dx12Wrapper> dx);

	void UpdateAndSetDrawData(std::shared_ptr<Dx12Wrapper> dx);

	void SetPmxActor(PMXActor* actor);
	void UpdatePostProcessMenu(std::shared_ptr<Dx12Wrapper> dx, std::shared_ptr<PMXRenderer> renderer);

private:
	void UpdatePmxActorDebugWindow();

private:
	float mFov = pi / 4.0f;
	float mLightRotation[3] = { 40.0f, 0.0f, 0.0f };

	int mBloomIteration = 8;
	float mBloomIntensity = 1.0f;

	PMXActor* _pmxActor = nullptr;
	std::vector<LoadMaterial> _editMaterials;

	static ImguiManager _instance;

	bool _enableBloom;
	bool _enableSSAO;
};

