#include"Application.h"
#include"Dx12Wrapper.h"
#include"PMDRenderer.h"
#include"PMDActor.h"
#include"PMXActor.h"
#include "PMXRenderer.h"
#include "ImguiManager.h"
#include "FBXActor.h"
#include "FBXRenderer.h"

#include "Imgui/imgui.h"
#include "Imgui/imgui_impl_dx12.h"
#include "Imgui/imgui_impl_win32.h"

#include "PmxFileData.h"
#include "Time.h"
#include "Define.h"
#include "Input.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	if (msg == WM_MOUSEMOVE)
	{
		Input::Instance()->SetWMMouse(static_cast<int>(LOWORD(lparam)), static_cast<int>(HIWORD(lparam)));
	}

	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Application::CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass)
{
	_hInstance = GetModuleHandle(nullptr);

	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure;
	windowClass.lpszClassName = _T("DX12Sample");
	windowClass.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&windowClass);

	RECT wrc = { 0, 0, window_width, window_height };

	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	hwnd = CreateWindow(windowClass.lpszClassName,
		_T("DX12test"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		windowClass.hInstance,
		nullptr);
}

bool Application::Init()
{
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);
	CreateGameWindow(_hwnd, _windowClass);

	_dx12.reset(new Dx12Wrapper(_hwnd));

	PhysicsManager::Create();
	PhysicsManager::ActivePhysics(true);

	_pmdRenderer.reset(new PMDRenderer(*_dx12));

	auto miku = std::make_shared<PMDActor>("Model/miku.pmd", *_dx12);
	miku->PlayAnimation();
	miku->SetPosition(0.f, 0.f, 0.f);
	_pmdRenderer->AddActor(miku);

	auto miku2 = std::make_shared<PMDActor>("Model/miku.pmd", *_dx12);
	miku2->PlayAnimation();
	miku2->SetPosition(20.f, 0.f, 50.f);
	_pmdRenderer->AddActor(miku2);

	_pmxRenderer.reset(new PMXRenderer(*_dx12));

	auto pmxMiku = std::make_shared<PMXActor>();
	bool bResult = pmxMiku.get()->Initialize(L"PMXModel\\«ß«¯ªµªó.pmx", *_dx12);
	if (bResult == false)
	{
		return false;
	}

	_pmxRenderer->AddActor(pmxMiku);

	auto stage = std::make_shared<FBXActor>();
	stage->Initialize("FBX/stage.fbx", *_dx12);

	_fbxRenderer.reset(new FBXRenderer(*_dx12));
	_fbxRenderer->AddActor(stage);

	bResult = ImguiManager::Instance().Initialize(_hwnd, _dx12);
	if (bResult == false)
	{
		return false;
	}

	//bResult = LoadPMXFile(L"PMXModel\\«ß«¯ªµªó.pmx");
	//	if (bResult == false)
	//{
	//	return false;
	//}

	return true;
}

void Application::Run()
{
	ShowWindow(_hwnd, SW_SHOW);
	float angle = 0.0f;
	MSG msg = {};
	unsigned int frame = 0;
	Time::Init();

	if (Input::Instance()->Initialize(_hInstance, _hwnd) == false)
	{
		OutputDebugStringA("Fail Input Initialize");
		return;
	}

	while (true)
	{
		Time::FrameTime();

		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
		{
			break;
		}

		if (Input::Instance()->Update() == false)
		{
			break;
		}

		_dx12->SetCameraSetting();

		static FBXActor* selectedFbxActor = nullptr;

		if (Input::Instance()->GetMouseDown(0) == true)
		{
			float mousePositionX = Input::Instance()->GetMousePositionX();
			float mousePositionY = Input::Instance()->GetMousePositionY();
			XMFLOAT3 cameraPosition = _dx12->GetCameraPosition();
			XMMATRIX viewMatrix = _dx12->GetViewMatrix();
			XMMATRIX projMatrix = _dx12->GetProjectionMatrix();

			for (auto& actor : _fbxRenderer->GetActor())
			{
				if (actor->TestSelect(mousePositionX, mousePositionY, cameraPosition, viewMatrix, projMatrix) == true)
				{
					selectedFbxActor = actor.get();
				}
			}
		}

		_pmxRenderer->Update();
		_pmxRenderer->BeforeDrawFromLight();

		_fbxRenderer->Update();

		_dx12->PreDrawShadow();

		_pmxRenderer->DrawFromLight();

		_dx12->PreDrawToPera1();

		_pmxRenderer->BeforeDrawAtDeferredPipeline();

		_dx12->DrawToPera1();

		_pmxRenderer->Draw();

		_fbxRenderer->BeforeDrawAtForwardPipeline();
		
		_dx12->DrawToPera1ForFbx();

		_fbxRenderer->Draw();

		_dx12->PostDrawToPera1();

		_dx12->DrawAmbientOcclusion();

		_dx12->DrawShrinkTextureForBlur();

		_dx12->Clear();

		_dx12->Draw();

		_dx12->Update();

		ImguiManager::Instance().StartUI();
		ImguiManager::Instance().UpdateAndSetDrawData(_dx12);
		ImguiManager::Instance().UpdatePostProcessMenu(_dx12, _pmxRenderer);
		ImguiManager::Instance().UpdateSelectInspector(selectedFbxActor);
		ImguiManager::Instance().EndUI(_dx12);

		_dx12->EndDraw();
	}
}

void Application::Terminate()
{
	PhysicsManager::Destroy();

	UnregisterClass(_windowClass.lpszClassName, _windowClass.hInstance);
}

SIZE Application::GetWindowSize() const
{
	SIZE ret;
	ret.cx = window_width;
	ret.cy = window_height;
	return ret;
}

Application::Application()
{

}

Application::~Application()
{
}

Application& Application::Instance()
{
	static Application instance;
	return instance;
}