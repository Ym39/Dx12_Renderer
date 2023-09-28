#include"Application.h"
#include"Dx12Wrapper.h"
#include"PMDRenderer.h"
#include"PMDActor.h"
#include"PMXActor.h"
#include "PMXRenderer.h"

#include "Imgui/imgui.h"
#include "Imgui/imgui_impl_dx12.h"
#include "Imgui/imgui_impl_win32.h"

const unsigned int window_width = 1600;
const unsigned int window_height = 800;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Application::CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass)
{
	HINSTANCE hInst = GetModuleHandle(nullptr);

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

std::wstring s2ws(const std::string& str)
{
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

bool Application::Init()
{
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);
	CreateGameWindow(_hwnd, _windowClass);

	_dx12.reset(new Dx12Wrapper(_hwnd));
	_pmdRenderer.reset(new PMDRenderer(*_dx12));

	auto miku = std::make_shared<PMDActor>("Model/miku.pmd", *_pmdRenderer);
	miku->PlayAnimation();
	miku->SetPosition(0.f, 0.f, 0.f);
	_pmdRenderer->AddActor(miku);

	auto miku2 = std::make_shared<PMDActor>("Model/miku.pmd", *_pmdRenderer);
	miku2->PlayAnimation();
	miku2->SetPosition(20.f, 0.f, 50.f);
	_pmdRenderer->AddActor(miku2);

	_pmxRenderer.reset(new PMXRenderer(*_dx12));
	_pmxActor.reset(new PMXActor(L"PMXModel\\«ß«¯ªµªó.pmx", *_pmxRenderer));

	if (ImGui::CreateContext() == nullptr)
	{
		assert(0);
		return false;
	}

	bool blnResult = ImGui_ImplWin32_Init(_hwnd);
	if (blnResult == false)
	{
		assert(0);
		return false;
	}

	blnResult = ImGui_ImplDX12_Init(_dx12->Device().Get(),
		3,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		_dx12->GetHeapForImgui().Get(),
		_dx12->GetHeapForImgui()->GetCPUDescriptorHandleForHeapStart(),
		_dx12->GetHeapForImgui()->GetGPUDescriptorHandleForHeapStart());

	return true;
}

void Application::Run()
{
	ShowWindow(_hwnd, SW_SHOW);
	float angle = 0.0f;
	MSG msg = {};
	unsigned int frame = 0;

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
		{
			break;
		}

		_dx12->SetCameraSetting();

		_pmdRenderer->Update();

		_pmdRenderer->BeforeDrawFromLight();

		_dx12->PreDrawShadow();

		_pmdRenderer->DrawFromLight();

		_dx12->PreDrawToPera1();

		_pmdRenderer->BeforeDraw();

		//_pmxActor->Update();

		_dx12->DrawToPera1();

		_pmdRenderer->Draw();

		//_pmxActor->BeforeDraw();
		//_pmxActor->Draw();

		_dx12->PostDrawToPera1();

		_dx12->DrawAmbientOcclusion();

		_dx12->DrawShrinkTextureForBlur();

		//_dx12->DrawBokeh();

		_dx12->Clear();

		_dx12->Draw();

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Rendering Test Menu");
		ImGui::SetWindowSize(ImVec2(400, 500), ImGuiCond_::ImGuiCond_FirstUseEver);

		constexpr float pi = 3.141592653589f;
		static float fov = pi / 4.0f;
		ImGui::SliderFloat("FOV", &fov, pi / 6.0f, pi * 5.0f / 6.0f);

		static float lightVec[3] = { 1.0f, -1.0f, 1.0f };
		ImGui::SliderFloat3("Light Vector", lightVec, -1.0f, 1.0f);

		ImGui::End();

		ImGui::Render();
		_dx12->CommandList()->SetDescriptorHeaps(1, _dx12->GetHeapForImgui().GetAddressOf());
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), _dx12->CommandList().Get());

		_dx12->EndDraw();

		_dx12->SetFov(fov);
		_dx12->SetLightVector(lightVec);
	}
}

void Application::Terminate()
{
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