#pragma once
#include "PmxFileData.h"

#include <memory>
#include <DirectXMath.h>

class BoneNode;
class NodeManager;

class MotionState;

class btRigidBody;
class btCollisionShape;
class btDiscreteDynamicsWorld;

class RigidBody
{
public:
	RigidBody();
	~RigidBody();

	bool Init(const PMXRigidBody& pmxRigidBody, NodeManager* nodeManager, BoneNode* boneNode);

	btRigidBody* GetRigidBody() const;
	unsigned short GetGroup() const;
	unsigned short GetGroupMask() const;

	void SetActive(bool active);
	void Reset(btDiscreteDynamicsWorld* world);
	void ResetTransform();
	void ReflectGlobalTransform();
	void CalcLocalTransform();

	DirectX::XMMATRIX& GetTransform();

private:
	enum class RigidBodyType
	{
		Kinematic,
		Dynamic,
		Aligned,
	};

private:
	std::unique_ptr<btCollisionShape> _shape;
	std::unique_ptr<MotionState> _activeMotionState;
	std::unique_ptr<MotionState> _kinematicMotionState;
	std::unique_ptr<btRigidBody> _rigidBody;

	RigidBodyType _rigidBodyType;
	unsigned short _group;
	unsigned short _groupMask;

	BoneNode* _node;
	DirectX::XMFLOAT4X4 _offsetMatrix;

	std::wstring _name;
};

