#include "Application.h"
#include <system_error>

#ifndef _DEBUG
int main()
{
#else
#include<Windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int){
#endif

	//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF)

	auto& app = Application::Instance();
	if (!app.Init()) {
		return -1;
	}
	app.Run();
	app.Terminate();
	return 0;
}
