#pragma once
#include "BoneNode.h"
#include "MathUtil.h"

#include <btBulletCollisionCommon.h>
#include "BulletCollision/CollisionDispatch/btCollisionDispatcherMt.h"
#include <btBulletDynamicsCommon.h>

class MotionState : public btMotionState
{
public:
	virtual void Reset() = 0;
	virtual void ReflectGlobalTransform() = 0;
};

class DefaultMotionState : public MotionState
{
public:
	DefaultMotionState(const XMMATRIX& transform)
	{
		float m[16];
		MathUtil::GetRowMajorMatrix(transform, m);
		_transform.setFromOpenGLMatrix(m);
		_initTransform = _transform;
	}

	void getWorldTransform(btTransform& worldTrans) const override
	{
		worldTrans = _transform;
	}

	void setWorldTransform(const btTransform& worldTrans) override
	{
		_transform = worldTrans;
	}

	void Reset() override
	{
		
	}

	void ReflectGlobalTransform() override
	{
		
	}

private:
	btTransform _initTransform;
	btTransform _transform;
};

class DynamicMotionState : public MotionState
{
public:
	DynamicMotionState(BoneNode* boneNode, const XMMATRIX& offset, bool override = true) :
	_boneNode(boneNode),
	_offset(offset),
	_override(override)
	{
		if (_boneNode->GetBoneIndex() == 136)
		{
			int d = 0;
		}

		XMVECTOR determinant = XMMatrixDeterminant(_offset);
		_invOffset = XMMatrixInverse(&determinant, _offset);

		Reset();
	}

	void getWorldTransform(btTransform& worldTransform) const override
	{
		if (_boneNode->GetBoneIndex() == 136)
		{
			int d = 0;
		}

		worldTransform = _transform;
	}

	void setWorldTransform(const btTransform& worldTrans) override
	{
		if (_boneNode->GetBoneIndex() == 136)
		{
			int d = 0;
		}

		_transform = worldTrans;
	}

	void Reset() override
	{
		if(_boneNode->GetBoneIndex() == 136)
		{
			int d = 0;
		}

		XMMATRIX global = _offset * _boneNode->GetGlobalTransform();
		float m[16];
		MathUtil::GetRowMajorMatrix(global, m);
		_transform.setFromOpenGLMatrix(m);
	}

	void ReflectGlobalTransform() override
	{

		if (_boneNode->GetBoneIndex() == 136)
		{
			int d = 0;
		}

		XMMATRIX world = MathUtil::GetMatrixFrombtTransform(_transform);
		XMMATRIX result = _invOffset * world;

		if (_override == true)
		{
			_boneNode->SetGlobalTransform(result);
			_boneNode->UpdateChildTransform();
		}
	}

private:
	BoneNode* _boneNode;
	XMMATRIX _offset;
	XMMATRIX _invOffset;
	btTransform _transform;
	bool _override;
};

class DynamicAndBoneMergeMotionState : public MotionState
{
public:
	DynamicAndBoneMergeMotionState(BoneNode* boneNode, const XMMATRIX& offset, bool override = true):
	_boneNode(boneNode),
	_offset(offset),
	_override(override)
	{
		XMVECTOR determinant = XMMatrixDeterminant(_offset);
		_invOffset = XMMatrixInverse(&determinant, offset);
		Reset();
	}

	void getWorldTransform(btTransform& worldTrans) const override
	{
		worldTrans = _transform;
	}

	void setWorldTransform(const btTransform& worldTrans) override
	{
		_transform = worldTrans;
	}

	void Reset() override
	{
		XMMATRIX global = _offset * _boneNode->GetGlobalTransform();
		float m[16];
		MathUtil::GetRowMajorMatrix(global, m);
		_transform.setFromOpenGLMatrix(m);
	}

	void ReflectGlobalTransform() override
	{
		XMMATRIX world = MathUtil::GetMatrixFrombtTransform(_transform);
		XMMATRIX result = _invOffset * world;
		XMMATRIX global = _boneNode->GetGlobalTransform();

		result.r[3] = global.r[3];

		if (_override == true)
		{
			_boneNode->SetGlobalTransform(result);
			_boneNode->UpdateChildTransform();
		}
	}

private:
	BoneNode* _boneNode;
	XMMATRIX _offset;
	XMMATRIX _invOffset;
	btTransform _transform;
	bool _override;
};

class KinematicMotionState : public MotionState
{
public:
	KinematicMotionState(BoneNode* node, const XMMATRIX& offset):
	_boneNode(node),
	_offset(offset)
	{
	}

	void getWorldTransform(btTransform& worldTrans) const override
	{
		XMMATRIX matrix;
		if (_boneNode != nullptr)
		{
			matrix = _offset * _boneNode->GetGlobalTransform();
		}
		else
		{
			matrix = _offset;
		}

		float m[16];
		MathUtil::GetRowMajorMatrix(matrix, m);
		worldTrans.setFromOpenGLMatrix(m);
	}

	void setWorldTransform(const btTransform& worldTrans) override
	{
	}

	void Reset() override
	{
	}

	void ReflectGlobalTransform() override
	{
	}

private:
	BoneNode* _boneNode;
	XMMATRIX _offset;
};

struct FilterCallback : public btOverlapFilterCallback
{
	bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override
	{
		auto findIterator = std::find_if(NonFilterProxy.begin(), NonFilterProxy.end(),
			[proxy0, proxy1](const auto& x)
			{
				return x == proxy0 || x == proxy1;
			});

		if (findIterator != NonFilterProxy.end())
		{
			return true;
		}

		bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
		collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);
		return collides;
	}

	std::vector<btBroadphaseProxy*> NonFilterProxy;
};
