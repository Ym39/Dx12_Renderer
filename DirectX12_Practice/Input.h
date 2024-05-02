#pragma once

#define DIRECTINPUT_VERSION 0x0800

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

#include <dinput.h>

class Input
{
public:
	static Input* Instance();

	bool Initialize(HINSTANCE hInstance, HWND hwnd);
	bool Update();
	void SetWMMouse(int x, int y);

	bool GetMouseDown(int index);
	bool GetMousePressed(int index);
	float GetMouseX() const;
	float GetMouseY() const;
	float GetMousePositionX() const;
	float GetMousePositionY() const;

	bool GetKeyPressed(int index);


private:
	Input();
	~Input();

	bool UpdateMouse();
	bool UpdateKeyBoard();

private:
	static Input* mInstance;

	IDirectInput8* mDirectInput = nullptr;
	IDirectInputDevice8* mKeyBoard = nullptr;
	IDirectInputDevice8* mMouse = nullptr;

	BYTE mMouseButtonGetKeyDownFlag[4] = { 0, };

	unsigned char mKeyBoardState[256];
	DIMOUSESTATE mMouseState;

	int mWMMouseX;
	int mWMMouseY;
};

