#pragma once
#include<Windows.h>
#include<tchar.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<DirectXMath.h>
#include<vector>
#include<map>
#include<d3dcompiler.h>
#include<DirectXTex.h>
#include<d3dx12.h>
#include<wrl.h>
#include<memory>

class Dx12Wrapper;
class PMDRenderer;
class PMDActor;
class PMXRenderer;
class PMXActor;
class FBXRenderer;
class InstancingRenderer;
class ImguiManager;
class MaterialManager;
class Render;

class Application
{
private:
	WNDCLASSEX _windowClass;
	HWND _hwnd;
	HINSTANCE _hInstance;
	std::shared_ptr<Dx12Wrapper> _dx12;
	std::shared_ptr<Render> _render = nullptr;

	void CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass);

	Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;

	void ReadSceneData();

public:
	static Application& Instance();

	bool Init();

	void Run();

	void Terminate();

	SIZE GetWindowSize() const;

	~Application();
};
