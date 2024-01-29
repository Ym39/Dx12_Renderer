#pragma once
#include<d3d12.h>
#include<Windows.h>
#include<memory>
#include<format>

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
	bool Initialize(HWND hwnd, std::shared_ptr<Dx12Wrapper> dx);
	void UpdateAndSetDrawData(std::shared_ptr<Dx12Wrapper> dx);

private:
	float mFov = pi / 4.0f;
	float mLightVector[3] = { 1.0f,-1.0f, 1.0f };
};

