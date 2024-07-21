#include "Time.h"

unsigned int Time::_applicationStartTime = 0;
unsigned int Time::_currentFrameTime = 0;
unsigned int Time::_deltaTime = 0.0f;
float Time::_deltaTimeFloat = 0.0f;
unsigned int Time::_startAnimationUpdateTime = 0;
unsigned int Time::_startMorphUpdateTime = 0;
unsigned int Time::_startSkinningUpdateTime = 0.0f;
unsigned int Time::_animationUpdateTime = 0;
unsigned int Time::_morphUpdateTime = 0;
unsigned int Time::_skinningUpdateTime = 0.0f;

void Time::Init()
{
	const unsigned int current = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	_currentFrameTime = current;
}

void Time::FrameTime()
{
	const unsigned int current = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	_deltaTime = current - _currentFrameTime;
	_deltaTimeFloat = static_cast<float>(_deltaTime) * 0.001f;
	_currentFrameTime = current;
}

unsigned int Time::GetTime()
{
	return _currentFrameTime;
}

float Time::GetTimeFloat()
{
	return static_cast<float>(_currentFrameTime) * 0.001f;
}

unsigned int Time::GetDeltaTime()
{
	return _deltaTime;
}

float Time::GetDeltaTimeFloat()
{
	return static_cast<float>(_deltaTime) * 0.001f;
}

unsigned Time::GetAnimationUpdateTime()
{
	return _animationUpdateTime;
}

unsigned Time::GetMorphUpdateTime()
{
	return _morphUpdateTime;
}

unsigned Time::GetSkinningUpdateTime()
{
	return _skinningUpdateTime;
}

void Time::RecordStartAnimationUpdateTime()
{
	_startAnimationUpdateTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void Time::RecordStartMorphUpdateTime()
{
	_startMorphUpdateTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void Time::RecordStartSkinningUpdateTime()
{
	_startSkinningUpdateTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void Time::EndAnimationUpdate()
{
	const unsigned int current = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	_animationUpdateTime = current - _startAnimationUpdateTime;
}

void Time::EndMorphUpdate()
{
	const unsigned int current = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	_morphUpdateTime = current - _startMorphUpdateTime;
}

void Time::EndSkinningUpdate()
{
	const unsigned int current = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	_skinningUpdateTime = current - _startSkinningUpdateTime;
}
