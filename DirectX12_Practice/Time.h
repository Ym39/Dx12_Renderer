#pragma once
#include <chrono>

class Time
{
public:
	static void Init();
	static void FrameTime();

	static unsigned int GetTime();
	static unsigned int GetDeltaTime();
	static float GetDeltaTimeFloat();

	static unsigned int GetAnimationUpdateTime();
	static unsigned int GetMorphUpdateTime();
	static unsigned int GetSkinningUpdateTime();

	static void RecordStartAnimationUpdateTime();
	static void RecordStartMorphUpdateTime();
	static void RecordStartSkinningUpdateTime();

	static void EndAnimationUpdate();
	static void EndMorphUpdate();
	static void EndSkinningUpdate();

private:
	static unsigned int _applicationStartTime;
	static unsigned int _currentFrameTime;
	static unsigned int _deltaTime;
	static float _deltaTimeFloat;

	static unsigned int _startAnimationUpdateTime;
	static unsigned int _startMorphUpdateTime;
	static unsigned int _startSkinningUpdateTime;

	static unsigned int _animationUpdateTime;
	static unsigned int _morphUpdateTime;
	static unsigned int _skinningUpdateTime;
};

