#include "RigidBody.h"
#include "MotionState.h"

#include "BoneNode.h"
#include "NodeManager.h"

RigidBody::RigidBody():
_shape(nullptr),
_activeMotionState(nullptr),
_kinematicMotionState(nullptr),
_rigidBody(nullptr),
_group(0),
_groupMask(0),
_node(nullptr)
{
}

RigidBody::~RigidBody()
{
}

bool RigidBody::Init(const PMXRigidBody& pmxRigidBody, NodeManager* nodeManager, BoneNode* boneNode)
{
	_shape = nullptr;

	if (boneNode->GetBoneIndex() == 30)
	{
		int d = 0;
	}

	switch (pmxRigidBody.shape)
	{
	case PMXRigidBody::Shape::Sphere:
		_shape = std::make_unique<btSphereShape>(pmxRigidBody.shapeSize.x);
		break;
	case PMXRigidBody::Shape::Box:
		_shape = std::make_unique<btBoxShape>(btVector3(pmxRigidBody.shapeSize.x, pmxRigidBody.shapeSize.y, pmxRigidBody.shapeSize.z));
		break;
	case PMXRigidBody::Shape::Capsule:
		_shape = std::make_unique<btCapsuleShape>(pmxRigidBody.shapeSize.x, pmxRigidBody.shapeSize.y);
		break;
	default:
		break;
	}

	if (_shape == nullptr)
	{
		return false;
	}

	btScalar mass(0.f);
	btVector3 localInertia(0.f, 0.f, 0.f);

	if (pmxRigidBody.op != PMXRigidBody::Operation::Static)
	{
		mass = pmxRigidBody.mass;
	}

	if (mass != 0.f)
	{
		_shape->calculateLocalInertia(mass, localInertia);
	}

	DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(pmxRigidBody.rotate.x, pmxRigidBody.rotate.y, pmxRigidBody.rotate.z);
	DirectX::XMMATRIX translate = DirectX::XMMatrixTranslation(pmxRigidBody.translate.x, pmxRigidBody.translate.y, pmxRigidBody.translate.z);

	DirectX::XMMATRIX rigidBodyMat = rotation * translate;

	BoneNode* kinematicNode = nullptr;
	if (boneNode != nullptr)
	{
		XMVECTOR determinant = XMMatrixDeterminant(boneNode->GetGlobalTransform());
		XMStoreFloat4x4(&_offsetMatrix, rigidBodyMat * XMMatrixInverse(&determinant, boneNode->GetGlobalTransform()));
		kinematicNode = boneNode;
	}
	else
	{
		BoneNode* root = nodeManager->GetBoneNodeByIndex(0);
		XMVECTOR determinant = XMMatrixDeterminant(boneNode->GetGlobalTransform());
		XMStoreFloat4x4(&_offsetMatrix, rigidBodyMat * XMMatrixInverse(&determinant, boneNode->GetGlobalTransform()));
		kinematicNode = root;
	}

	XMMATRIX xmOffsetMatrix = XMLoadFloat4x4(&_offsetMatrix);
	btMotionState* motionState = nullptr;
	if (pmxRigidBody.op == PMXRigidBody::Operation::Static)
	{
		_kinematicMotionState = std::make_unique<KinematicMotionState>(kinematicNode, xmOffsetMatrix);
		motionState = _kinematicMotionState.get();
	}
	else
	{
		if (boneNode != nullptr)
		{
			if (pmxRigidBody.op == PMXRigidBody::Operation::Dynamic)
			{
				_activeMotionState = std::make_unique<DynamicMotionState>(kinematicNode, xmOffsetMatrix);
				_kinematicMotionState = std::make_unique<KinematicMotionState>(kinematicNode, xmOffsetMatrix);
				motionState = _activeMotionState.get();
			}
			else if (pmxRigidBody.op == PMXRigidBody::Operation::DynamicAndBoneMerge)
			{
				_activeMotionState = std::make_unique<DynamicAndBoneMergeMotionState>(kinematicNode, xmOffsetMatrix);
				_kinematicMotionState = std::make_unique<KinematicMotionState>(kinematicNode, xmOffsetMatrix);
				motionState = _activeMotionState.get();
			}
		}
		else
		{
			_activeMotionState = std::make_unique<DefaultMotionState>(xmOffsetMatrix);
			_kinematicMotionState = std::make_unique<KinematicMotionState>(kinematicNode, xmOffsetMatrix);
			motionState = _activeMotionState.get();
		}
	}

	btRigidBody::btRigidBodyConstructionInfo rigidBodyInfo(mass, motionState, _shape.get(), localInertia);
	rigidBodyInfo.m_linearDamping = pmxRigidBody.translateDimmer;
	rigidBodyInfo.m_angularDamping = pmxRigidBody.rotateDimmer;
	rigidBodyInfo.m_restitution = pmxRigidBody.repulsion;
	rigidBodyInfo.m_friction = pmxRigidBody.friction;
	rigidBodyInfo.m_additionalDamping = true;

	_rigidBody = std::make_unique<btRigidBody>(rigidBodyInfo);
	_rigidBody->setUserPointer(this);
	_rigidBody->setSleepingThresholds(0.01f, XMConvertToRadians(0.1f));
	_rigidBody->setActivationState(DISABLE_DEACTIVATION);
	if (pmxRigidBody.op == PMXRigidBody::Operation::Static)
	{
		_rigidBody->setCollisionFlags(_rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	}

	_rigidBodyType = (RigidBodyType)pmxRigidBody.op;
	_group = pmxRigidBody.group;
	_groupMask = pmxRigidBody.collisionGroup;
	_node = boneNode;
	_name = pmxRigidBody.name;

	return true;
}

btRigidBody* RigidBody::GetRigidBody() const
{
	return _rigidBody.get();
}

unsigned short RigidBody::GetGroup() const
{
	return _group;
}

unsigned short RigidBody::GetGroupMask() const
{
	return _groupMask;
}

void RigidBody::SetActive(bool active)
{
	if (_rigidBodyType != RigidBodyType::Kinematic)
	{
		if (active == true)
		{
			//_rigidBody->setCollisionFlags(_rigidBody->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
			_rigidBody->setMotionState(_activeMotionState.get());
		}
		else
		{
			//_rigidBody->setCollisionFlags(_rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			_rigidBody->setMotionState(_kinematicMotionState.get());
		}
	}
	else
	{
		_rigidBody->setMotionState(_kinematicMotionState.get());
	}
}

void RigidBody::Reset(btDiscreteDynamicsWorld* world)
{
	btOverlappingPairCache* cache = world->getPairCache();
	if (cache != nullptr)
	{
		btDispatcher* dispatcher = world->getDispatcher();
		cache->cleanProxyFromPairs(_rigidBody->getBroadphaseHandle(), dispatcher);
	}

	_rigidBody->setAngularVelocity(btVector3(0, 0, 0));
	_rigidBody->setLinearVelocity(btVector3(0, 0, 0));
	_rigidBody->clearForces();
}

void RigidBody::ResetTransform()
{
	if (_activeMotionState != nullptr)
	{
		_activeMotionState->Reset();
	}
}

void RigidBody::ReflectGlobalTransform()
{
	if (_activeMotionState != nullptr)
	{
		_activeMotionState->ReflectGlobalTransform();
	}

	if (_kinematicMotionState != nullptr)
	{
		_kinematicMotionState->ReflectGlobalTransform();
	}
}

void RigidBody::CalcLocalTransform()
{
	if (_node == nullptr)
	{
		return;
	}

	const BoneNode* parent = _node->GetParentBoneNode();
	if (parent == nullptr)
	{
		_node->SetLocalTransform(_node->GetGlobalTransform());
	}
	else
	{
		XMVECTOR determinant = XMMatrixDeterminant(parent->GetGlobalTransform());
		XMMATRIX local = _node->GetGlobalTransform() * XMMatrixInverse(&determinant, parent->GetGlobalTransform());
		_node->SetLocalTransform(local);
	}
}

DirectX::XMMATRIX& RigidBody::GetTransform()
{
	btTransform transform = _rigidBody->getCenterOfMassTransform();
	return MathUtil::GetMatrixFrombtTransform(transform);
}




