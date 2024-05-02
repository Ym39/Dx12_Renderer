#pragma once
enum class TypeIdentity
{
	PMXActor,
	FbxActor
};

class IType
{
public:
	virtual TypeIdentity GetType() = 0;
};
