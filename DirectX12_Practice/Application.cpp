#include"Application.h"
#include"Dx12Wrapper.h"
#include"PMDRenderer.h"
#include"PMDActor.h"
#include"PMXActor.h"
#include "PMXRenderer.h"
#include "ImguiManager.h"
#include "InstancingRenderer.h"
#include "GeometryInstancingActor.h"
#include "FBXActor.h"
#include "FBXRenderer.h"
#include "MaterialManager.h"
#include "Geometry.h"
#include "Render.h"

#include "Imgui/imgui.h"
#include "Imgui/imgui_impl_dx12.h"
#include "Imgui/imgui_impl_win32.h"

#include "PmxFileData.h"
#include "Time.h"
#include "Define.h"
#include "Input.h"

#include <iomanip>
#include <fstream>


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
	_render.reset(new Render(_dx12));

	PhysicsManager::Create();
	PhysicsManager::ActivePhysics(true);

	auto pmxMiku = std::make_shared<PMXActor>();
	bool bResult = pmxMiku.get()->Initialize(L"PMXModel\\«ß«¯ªµªó.pmx", *_dx12);
	if (bResult == false)
	{
		return false;
	}

	pmxMiku->SetName("Miku");
	_render->AddPMXActor(pmxMiku);

	auto cubeGeometryActor = std::make_shared<GeometryInstancingActor>(Geometry::Cube(), 2000);
	cubeGeometryActor->GetTransform().SetPosition(-250.0f, 0.0f, -450.0f);
	cubeGeometryActor->GetTransform().SetScale(0.8f, 4.0f, 0.8f);

	cubeGeometryActor->Initialize(*_dx12);
	cubeGeometryActor->SetName("Cube");

	_render->AddGeometryInstancingActor(cubeGeometryActor);

	bResult = MaterialManager::Instance().Init(*_dx12);
	if (bResult == false)
	{
		return false;
	}

	ReadSceneData();

	bResult = ImguiManager::Instance().Initialize(_hwnd, _dx12);
	if (bResult == false)
	{
		return false;
	}

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

		//if (Input::Instance()->GetMouseDown(0) == true)
		//{
		//	float mousePositionX = Input::Instance()->GetMousePositionX();
		//	float mousePositionY = Input::Instance()->GetMousePositionY();
		//	XMFLOAT3 cameraPosition = _dx12->GetCameraPosition();
		//	XMMATRIX viewMatrix = _dx12->GetViewMatrix();
		//	XMMATRIX projMatrix = _dx12->GetProjectionMatrix();

		//	for (auto& actor : _fbxRenderer->GetActor())
		//	{
		//		if (actor->TestSelect(mousePositionX, mousePositionY, cameraPosition, viewMatrix, projMatrix) == true)
		//		{
		//			selectedFbxActor = actor.get();
		//		}
		//	}
		//}

		_render->Frame();
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

void Application::ReadSceneData()
{
	const std::string sceneJsonFileName = "Scene.json";

	std::ifstream sceneFile(sceneJsonFileName);
	if (sceneFile.good() == false)
	{
		return;
	}

	if (sceneFile.is_open() == false)
	{
		return;
	}

	json sceneData = json::parse(sceneFile);
	sceneFile.close();

	if (sceneData.contains("Directional Light") == true)
	{
		float lightDirection[3]
		{
			sceneData["Directional Light"]["x"],
			sceneData["Directional Light"]["y"],
			sceneData["Directional Light"]["z"]
		};

		_dx12->SetDirectionalLightRotation(lightDirection);
	}

	if (sceneData.contains("FbxActor") == true)
	{
		for (const auto& fbxActorJson : sceneData["FbxActor"])
		{
			if (fbxActorJson.contains("ModelPath") == false ||
				fbxActorJson.contains("Transform") == false)
			{
				continue;
			}

			std::string fbxFileName = fbxActorJson["ModelPath"];
			std::string name = "";
			if (fbxActorJson.contains("Name") == true)
			{
				name = fbxActorJson["Name"].get<std::string>();
			}

			json transformJson = fbxActorJson["Transform"];
			if (transformJson.contains("Position") == false ||
				transformJson.contains("Rotation") == false ||
				transformJson.contains("Scale") == false)
			{
				continue;
			}

			DirectX::XMFLOAT3 position;
			position.x = transformJson["Position"]["x"];
			position.y = transformJson["Position"]["y"];
			position.z = transformJson["Position"]["z"];

			DirectX::XMFLOAT3 rotation;
			rotation.x = transformJson["Rotation"]["x"];
			rotation.y = transformJson["Rotation"]["y"];
			rotation.z = transformJson["Rotation"]["z"];

			DirectX::XMFLOAT3 scale;
			scale.x = transformJson["Scale"]["x"];
			scale.y = transformJson["Scale"]["y"];
			scale.z = transformJson["Scale"]["z"];

			std::shared_ptr<FBXActor> newActor = std::make_shared<FBXActor>();
			if (newActor->Initialize(fbxFileName, *_dx12) == false)
			{
				continue;
			}
			Transform& transform = newActor->GetTransform();
			transform.SetPosition(position.x, position.y, position.z);
			transform.SetRotation(rotation.x, rotation.y, rotation.z);
			transform.SetScale(scale.x, scale.y, scale.z);

			if (fbxActorJson.contains("Material") == true)
			{
				std::vector<std::string> materialNameList;
				json materialJson = fbxActorJson["Material"];
				materialNameList.resize(materialJson.size());

				for (int i = 0; i < materialNameList.size(); i++)
				{
					materialNameList[i] = materialJson[i].get<std::string>();
				}

				newActor->SetMaterialName(materialNameList);
			}

			newActor->SetName(name);

			//_fbxRenderer->AddActor(newActor);
			_render->AddFBXActor(newActor);
		}
	}
}

Application& Application::Instance()
{
	static Application instance;
	return instance;
}
