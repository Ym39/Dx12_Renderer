#include "Input.h"
#include "Define.h"

Input* Input::mInstance = nullptr;

Input* Input::Instance()
{
	if (mInstance == nullptr)
	{
		mInstance = new Input();
	}

	return mInstance;
}

bool Input::Initialize(HINSTANCE hInstance, HWND hwnd)
{
	HRESULT result = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void**>(&mDirectInput), nullptr);
	if (FAILED(result) == true)
	{
		return false;
	}

	result = mDirectInput->CreateDevice(GUID_SysKeyboard, &mKeyBoard, nullptr);
	if (FAILED(result) == true)
	{
		return false;
	}

	result = mKeyBoard->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(result) == true)
	{
		return false;
	}

	result = mKeyBoard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if (FAILED(result) == true)
	{
		return false;
	}

	result = mKeyBoard->Acquire();
	if (FAILED(result) == true)
	{
		return false;
	}

	result = mDirectInput->CreateDevice(GUID_SysMouse, &mMouse, nullptr);
	if (FAILED(result) == true)
	{
		return false;
	}

	result = mMouse->SetDataFormat(&c_dfDIMouse);
	if (FAILED(result) == true)
	{
		return false;
	}

	result = mMouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if (FAILED(result) == true)
	{
		return false;
	}

	result = mMouse->Acquire();
	if (FAILED(result) == true)
	{
		return false;
	}

	return true;
}

bool Input::Update()
{
	bool result = UpdateMouse();
	if (result == false)
	{
		return result;
	}

	result = UpdateKeyBoard();
	if (result == false)
	{
		return result;
	}

	return result;
}

void Input::SetWMMouse(int x, int y)
{
	mWMMouseX = x;
	mWMMouseY = y;
}

bool Input::GetMouseDown(int index)
{
	if (index < 0 || index >= 4)
	{
		return false;
	}

	if (mMouseButtonGetKeyDownFlag[index] == 1)
	{
		return false;
	}

	if (mMouseState.rgbButtons[index] & 0x80)
	{
		mMouseButtonGetKeyDownFlag[index] = 1;
		return true;
	}

	return false;
}

bool Input::GetMousePressed(int index)
{
	if (index < 0 || index >= 4)
	{
		return false;
	}

	if (mMouseState.rgbButtons[index] & 0x80)
	{
		return true;
	}

	return false;
}

float Input::GetMouseX() const
{
	return static_cast<float>(mMouseState.lX);
}

float Input::GetMouseY() const
{
	return static_cast<float>(mMouseState.lY);
}

float Input::GetMousePositionX() const
{
	return mWMMouseX;
}

float Input::GetMousePositionY() const
{
	return mWMMouseY;
}

bool Input::GetKeyPressed(int index)
{
	if (index < 0 || index >= 256)
	{
		return false;
	}

	if (mKeyBoardState[index] & 0x80)
	{
		return true;
	}

	return false;
}

bool Input::UpdateMouse()
{
	HRESULT result = mMouse->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&mMouseState);
	if (FAILED(result) == true)
	{
		if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED)
		{
			mMouse->Acquire();
		}
		else
		{
			return false;
		}
	}

	for (int i = 0; i < 4; i++)
	{
		if ((mMouseState.rgbButtons[i] & 0x80) == false)
		{
			mMouseButtonGetKeyDownFlag[i] = 0;
		}
	}

	return true;
}

bool Input::UpdateKeyBoard()
{
	HRESULT result = mKeyBoard->GetDeviceState(sizeof(mKeyBoardState), (LPVOID)&mKeyBoardState);
	if (FAILED(result) == true)
	{
		if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED)
		{
			mKeyBoard->Acquire();
		}
		else
		{
			return false;
		}
	}

	return true;
}

Input::Input()
{
}

Input::~Input()
{
	if (mMouse != nullptr)
	{
		mMouse->Unacquire();
		mMouse->Release();
		mMouse = nullptr;
	}

	if (mKeyBoard != nullptr)
	{
		mKeyBoard->Unacquire();
		mKeyBoard->Release();
		mKeyBoard = nullptr;
	}

	if (mDirectInput != nullptr)
	{
		mDirectInput->Release();
		mDirectInput = nullptr;
	}
}
