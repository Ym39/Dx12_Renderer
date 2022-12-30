#pragma once

class Dx12Wrapper;

class Application
{
private:
	WNDCLASSEX _windowClass;
	HWND _hwnd;

	Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;

public:
	static Application& Instance();

	bool Init();

	void Run();

	void Terminate();

	SIZE GetWindowSize() const;

	~Application();
};

