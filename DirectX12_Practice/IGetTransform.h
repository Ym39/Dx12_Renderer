#pragma once

class Transform;
class IGetTransform
{
public:
	virtual ~IGetTransform() = default;

private:
	virtual Transform& GetTransform() = 0;
};

