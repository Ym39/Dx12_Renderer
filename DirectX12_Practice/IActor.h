#pragma once
#include <string>

class Dx12Wrapper;

class IActor
{
public:
	virtual ~IActor() = default;
	virtual void Initialize(Dx12Wrapper& dx) = 0;
	virtual void Draw(Dx12Wrapper& dx, bool isShadow) const = 0;
	virtual void Update() = 0;
	virtual void EndOfFrame(Dx12Wrapper& dx) = 0;
	virtual int GetIndexCount() const = 0;
	virtual std::string GetName() const = 0;
	virtual void SetName(std::string name) = 0;
	virtual void UpdateImGui(Dx12Wrapper& dx) = 0;
};

