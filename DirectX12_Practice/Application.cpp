#include"Application.h"
#include"Dx12Wrapper.h"
#include"PMDRenderer.h"
#include"PMDActor.h"

const unsigned int window_width = 1600;
const unsigned int window_height = 800;

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

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


bool Application::Init()
{
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);
	CreateGameWindow(_hwnd, _windowClass);

	_dx12.reset(new Dx12Wrapper(_hwnd));
	_pmdRenderer.reset(new PMDRenderer(*_dx12));
	_pmdActor.reset(new PMDActor("Model/miku.pmd", *_pmdRenderer));

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

		_dx12->BeginDraw();

		_dx12->CommandList()->SetPipelineState(_pmdRenderer->GetPipelineState());

		_dx12->CommandList()->SetGraphicsRootSignature(_pmdRenderer->GetRootSignature());

		_dx12->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		_dx12->SetScene();

		_pmdActor->Update();
		_pmdActor->Draw();

		_dx12->EndDraw();

		_dx12->Swapchain()->Present(1, 0);
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