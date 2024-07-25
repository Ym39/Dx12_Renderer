#pragma once
#include <string>

class Dx12Wrapper;

class IActor
{
public:
	virtual std::string GetName() const = 0;
	virtual void SetName(std::string name) = 0;
	virtual void UpdateImGui(Dx12Wrapper& dx) = 0;
};

