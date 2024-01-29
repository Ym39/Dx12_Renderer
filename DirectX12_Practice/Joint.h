#pragma once
#include "PmxFileData.h"

class RigidBody;
class btTypedConstraint;

class Joint
{
public:
	Joint();
	~Joint();
	Joint(const Joint& other) = delete;
	Joint& operator = (const Joint& other) = delete;

	bool CreateJoint(const PMXJoint& pmxJoint, RigidBody* rigidBody0, RigidBody* rigidBody1);
	void Destroy();

	btTypedConstraint* GetConstraint() const;

private:
	std::unique_ptr<btTypedConstraint> _constraint;
};

