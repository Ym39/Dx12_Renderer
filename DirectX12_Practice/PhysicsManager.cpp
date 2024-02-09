#include "PhysicsManager.h"
#include "MotionState.h"
#include "RigidBody.h"
#include "Joint.h"
#include "PMXActor.h"

#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolverMt.h>
#include "BulletDynamics/MLCPSolvers/btDantzigSolver.h"
#include "BulletDynamics/Dynamics/btDiscreteDynamicsWorldMt.h"
#include "BulletDynamics/ConstraintSolver/btContactSolverInfo.h"

std::unique_ptr<btDiscreteDynamicsWorld> PhysicsManager::_world = nullptr;
std::unique_ptr<btBroadphaseInterface> PhysicsManager::_broadPhase = nullptr;
std::unique_ptr<btDefaultCollisionConfiguration> PhysicsManager::_collisionConfig = nullptr;
std::unique_ptr<btCollisionDispatcher> PhysicsManager::_dispatcher = nullptr;
std::unique_ptr<btSequentialImpulseConstraintSolver> PhysicsManager::_solver = nullptr;
std::unique_ptr<btCollisionShape> PhysicsManager::_groundShape = nullptr;
std::unique_ptr<btMotionState> PhysicsManager::_groundMS = nullptr;
std::unique_ptr<btRigidBody> PhysicsManager::_groundRB = nullptr;
std::unique_ptr<btOverlapFilterCallback> PhysicsManager::_filterCB = nullptr;

float PhysicsManager::_fixedTimeStep = 0.033333f;
int PhysicsManager::_maxSubStepCount = 4;

std::thread PhysicsManager::_physicsUpdateThread = std::thread();
bool PhysicsManager::_threadFlag = false;

std::atomic<bool> PhysicsManager::_stopFlag(false);
std::atomic<bool> PhysicsManager::_endFlag(false);

PhysicsManager::PhysicsManager()
{

}

PhysicsManager::~PhysicsManager()
{

}

bool PhysicsManager::Create()
{
	_broadPhase = std::make_unique<btDbvtBroadphase>();
	_collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
	_dispatcher = std::make_unique<btCollisionDispatcher>(_collisionConfig.get());
	_solver = std::make_unique<btSequentialImpulseConstraintSolver>();

	_world = std::make_unique<btDiscreteDynamicsWorld>(_dispatcher.get(), _broadPhase.get(), _solver.get(), _collisionConfig.get());
	_world->setGravity(btVector3(0, -9.8f * 10.0f, 0));

	_groundShape = std::make_unique<btStaticPlaneShape>(btVector3(0.f, 1.f, 0.f), 0.f);

	btTransform groundTransform;
	groundTransform.setIdentity();

	_groundMS = std::make_unique<btDefaultMotionState>(groundTransform);

	btRigidBody::btRigidBodyConstructionInfo groundInfo(0.f, _groundMS.get(), _groundShape.get(), btVector3(0.f, 0.f, 0.f));
	_groundRB = std::make_unique<btRigidBody>(groundInfo);

	_world->addRigidBody(_groundRB.get());

	auto filterCB = std::make_unique<FilterCallback>();
	filterCB->NonFilterProxy.push_back(_groundRB->getBroadphaseProxy());
	_world->getPairCache()->setOverlapFilterCallback(filterCB.get());
	_filterCB = std::move(filterCB);

	btContactSolverInfo& info = _world->getSolverInfo();
	info.m_numIterations = 10;
	info.m_solverMode = SOLVER_SIMD;

	return true;
}

void PhysicsManager::Destroy()
{
	if (_threadFlag == true)
	{
		_stopFlag.store(true);

		while (_endFlag.load() == false);
	}

	if (_world != nullptr && _groundRB != nullptr)
	{
		_world->removeRigidBody(_groundRB.get());
	}

	_world = nullptr;
	_broadPhase = nullptr;
	_collisionConfig = nullptr;
	_dispatcher = nullptr;
	_solver = nullptr;
	_groundShape = nullptr;
	_groundMS = nullptr;
	_groundRB = nullptr;
}

void PhysicsManager::SetMaxSubStepCount(int numSteps)
{
	_maxSubStepCount = numSteps;
}

int PhysicsManager::GetMaxSubStepCount()
{
	return _maxSubStepCount;
}

void PhysicsManager::SetFixedTimeStep(float fixedTimeStep)
{
	_fixedTimeStep = fixedTimeStep;
}

float PhysicsManager::GetFixedTimeStep()
{
	return _fixedTimeStep;
}

void PhysicsManager::ActivePhysics(bool active)
{
	if (active == true)
	{
		_stopFlag.store(false);
		_endFlag.store(false);
		_threadFlag = true;
		_physicsUpdateThread = std::thread(UpdateByThread);
		_physicsUpdateThread.detach();
	}
	else
	{
		_stopFlag.store(true);
		while (_endFlag.load() == false);
		_threadFlag = false;
	}
}

void PhysicsManager::ForceUpdatePhysics()
{
	_world->stepSimulation(_fixedTimeStep, _maxSubStepCount, _fixedTimeStep);
}

void PhysicsManager::AddRigidBody(RigidBody* rigidBody)
{
	_world->addRigidBody(rigidBody->GetRigidBody(), 1 << rigidBody->GetGroup(), rigidBody->GetGroupMask());
}

void PhysicsManager::RemoveRigidBody(RigidBody* rigidBody)
{
	_world->removeRigidBody(rigidBody->GetRigidBody());
}

void PhysicsManager::AddJoint(Joint* joint)
{
	if (joint->GetConstraint() == nullptr)
	{
		return;
	}

	_world->addConstraint(joint->GetConstraint());
}

void PhysicsManager::RemoveJoint(Joint* joint)
{
	if (joint->GetConstraint() == nullptr)
	{
		return;
	}

	_world->removeConstraint(joint->GetConstraint());
}

btDiscreteDynamicsWorld* PhysicsManager::GetDynamicsWorld()
{
	return _world.get();
}

void PhysicsManager::UpdateByThread()
{
	unsigned long prevTime = timeGetTime();

	while (true)
	{
		if (_stopFlag.load() == true)
		{
			break;
		}

		unsigned long currentTime = timeGetTime();
		unsigned long deltaTime = currentTime - prevTime;
		prevTime = currentTime;

		_world->stepSimulation(deltaTime * 0.001f, _maxSubStepCount, _fixedTimeStep);
	}

	_endFlag.store(true);
}






