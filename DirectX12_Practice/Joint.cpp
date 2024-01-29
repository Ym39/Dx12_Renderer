#include "Joint.h"
#include "RigidBody.h"

#include <btBulletDynamicsCommon.h>

Joint::Joint()
{
}

Joint::~Joint()
{
}

bool Joint::CreateJoint(const PMXJoint& pmxJoint, RigidBody* rigidBody0, RigidBody* rigidBody1)
{
	Destroy();

	btMatrix3x3 rotation;
	rotation.setEulerZYX(pmxJoint.rotate.x, pmxJoint.rotate.y, pmxJoint.rotate.z);

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(pmxJoint.translate.x, pmxJoint.translate.y, pmxJoint.translate.z));
	transform.setBasis(rotation);

	btTransform inv0 = rigidBody0->GetRigidBody()->getWorldTransform().inverse();
	btTransform inv1 = rigidBody1->GetRigidBody()->getWorldTransform().inverse();
	inv0 = inv0 * transform;
	inv1 = inv1 * transform;

	auto constraint = std::make_unique<btGeneric6DofSpringConstraint>(*rigidBody0->GetRigidBody(), *rigidBody1->GetRigidBody(), inv0, inv1, true);
	constraint->setLinearLowerLimit(btVector3(pmxJoint.translateLowerLimit.x, pmxJoint.translateLowerLimit.y, pmxJoint.translateLowerLimit.z));
	constraint->setLinearUpperLimit(btVector3(pmxJoint.translateUpperLimit.x, pmxJoint.translateUpperLimit.y, pmxJoint.translateUpperLimit.z));
	constraint->setAngularLowerLimit(btVector3(pmxJoint.rotateLowerLimit.x, pmxJoint.rotateLowerLimit.y, pmxJoint.rotateLowerLimit.z));
	constraint->setAngularUpperLimit(btVector3(pmxJoint.rotateUpperLimit.x, pmxJoint.rotateUpperLimit.y, pmxJoint.rotateUpperLimit.z));

	if (pmxJoint.springTranslateFactor.x != 0)
	{
		constraint->enableSpring(0, true);
		constraint->setStiffness(0, pmxJoint.springTranslateFactor.x);
	}

	if (pmxJoint.springTranslateFactor.y != 0)
	{
		constraint->enableSpring(1, true);
		constraint->setStiffness(1, pmxJoint.springTranslateFactor.y);
	}

	if (pmxJoint.springTranslateFactor.z != 0)
	{
		constraint->enableSpring(2, true);
		constraint->setStiffness(2, pmxJoint.springTranslateFactor.z);
	}

	if (pmxJoint.springRotateFactor.x != 0)
	{
		constraint->enableSpring(3, true);
		constraint->setStiffness(3, pmxJoint.springRotateFactor.x);
	}

	if (pmxJoint.springRotateFactor.y != 0)
	{
		constraint->enableSpring(4, true);
		constraint->setStiffness(4, pmxJoint.springRotateFactor.y);
	}

	if (pmxJoint.springRotateFactor.z != 0)
	{
		constraint->enableSpring(5, true);
		constraint->setStiffness(5, pmxJoint.springRotateFactor.z);
	}

	_constraint = std::move(constraint);

	return true;
}

void Joint::Destroy()
{
	_constraint = nullptr;
}

btTypedConstraint* Joint::GetConstraint() const
{
	return _constraint.get();
}
