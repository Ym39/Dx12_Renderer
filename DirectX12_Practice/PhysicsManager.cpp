#include "PhysicsManager.h"
#include "MotionState.h"
#include "RigidBody.h"
#include "Joint.h"
#include "PMXActor.h"

#include <BulletDynamics/ConstraintSolver/btSequentialImpulseConstraintSolverMt.h>
#include "BulletDynamics/MLCPSolvers/btDantzigSolver.h"
#include "BulletDynamics/Dynamics/btDiscreteDynamicsWorldMt.h"
#include "BulletDynamics/ConstraintSolver/btContactSolverInfo.h"

std::unique_ptr<btDiscreteDynamicsWorld> PhysicsManager::mWorld = nullptr;
std::unique_ptr<btBroadphaseInterface> PhysicsManager::mBroadPhase = nullptr;
std::unique_ptr<btDefaultCollisionConfiguration> PhysicsManager::mCollisionConfig = nullptr;
std::unique_ptr<btCollisionDispatcher> PhysicsManager::mDispatcher = nullptr;
std::unique_ptr<btSequentialImpulseConstraintSolver> PhysicsManager::mSolver = nullptr;
std::unique_ptr<btCollisionShape> PhysicsManager::mGroundShape = nullptr;
std::unique_ptr<btMotionState> PhysicsManager::mGroundMS = nullptr;
std::unique_ptr<btRigidBody> PhysicsManager::mGroundRB = nullptr;
std::unique_ptr<btOverlapFilterCallback> PhysicsManager::mFilterCB = nullptr;

float PhysicsManager::mFixedTimeStep = 0.033333f;
int PhysicsManager::mMaxSubStepCount = 4;

std::thread PhysicsManager::mPhysicsUpdateThread = std::thread();
bool PhysicsManager::mThreadFlag = false;

std::atomic<bool> PhysicsManager::mStopFlag(false);
std::atomic<bool> PhysicsManager::mEndFlag(false);

PhysicsManager::PhysicsManager()
{

}

PhysicsManager::~PhysicsManager()
{

}

bool PhysicsManager::Create()
{
	mBroadPhase = std::make_unique<btDbvtBroadphase>();
	mCollisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
	mDispatcher = std::make_unique<btCollisionDispatcher>(mCollisionConfig.get());
	mSolver = std::make_unique<btSequentialImpulseConstraintSolver>();

	mWorld = std::make_unique<btDiscreteDynamicsWorld>(mDispatcher.get(), mBroadPhase.get(), mSolver.get(), mCollisionConfig.get());
	mWorld->setGravity(btVector3(0, -9.8f * 10.0f, 0));

	mGroundShape = std::make_unique<btStaticPlaneShape>(btVector3(0.f, 1.f, 0.f), 0.f);

	btTransform groundTransform;
	groundTransform.setIdentity();

	mGroundMS = std::make_unique<btDefaultMotionState>(groundTransform);

	btRigidBody::btRigidBodyConstructionInfo groundInfo(0.f, mGroundMS.get(), mGroundShape.get(), btVector3(0.f, 0.f, 0.f));
	mGroundRB = std::make_unique<btRigidBody>(groundInfo);

	mWorld->addRigidBody(mGroundRB.get());

	auto filterCB = std::make_unique<FilterCallback>();
	filterCB->NonFilterProxy.push_back(mGroundRB->getBroadphaseProxy());
	mWorld->getPairCache()->setOverlapFilterCallback(filterCB.get());
	mFilterCB = std::move(filterCB);

	btContactSolverInfo& info = mWorld->getSolverInfo();
	info.m_numIterations = 10;
	info.m_solverMode = SOLVER_SIMD;

	return true;
}

void PhysicsManager::Destroy()
{
	if (mThreadFlag == true)
	{
		mStopFlag.store(true);

		while (mEndFlag.load() == false);
	}

	if (mWorld != nullptr && mGroundRB != nullptr)
	{
		mWorld->removeRigidBody(mGroundRB.get());
	}

	mWorld = nullptr;
	mBroadPhase = nullptr;
	mCollisionConfig = nullptr;
	mDispatcher = nullptr;
	mSolver = nullptr;
	mGroundShape = nullptr;
	mGroundMS = nullptr;
	mGroundRB = nullptr;
}

void PhysicsManager::SetMaxSubStepCount(int numSteps)
{
	mMaxSubStepCount = numSteps;
}

int PhysicsManager::GetMaxSubStepCount()
{
	return mMaxSubStepCount;
}

void PhysicsManager::SetFixedTimeStep(float fixedTimeStep)
{
	mFixedTimeStep = fixedTimeStep;
}

float PhysicsManager::GetFixedTimeStep()
{
	return mFixedTimeStep;
}

void PhysicsManager::ActivePhysics(bool active)
{
	if (active == true)
	{
		mStopFlag.store(false);
		mEndFlag.store(false);
		mThreadFlag = true;
		mPhysicsUpdateThread = std::thread(UpdateByThread);
		mPhysicsUpdateThread.detach();
	}
	else
	{
		mStopFlag.store(true);
		while (mEndFlag.load() == false);
		mThreadFlag = false;
	}
}

void PhysicsManager::ForceUpdatePhysics()
{
	mWorld->stepSimulation(mFixedTimeStep, mMaxSubStepCount, mFixedTimeStep);
}

void PhysicsManager::AddRigidBody(RigidBody* rigidBody)
{
	mWorld->addRigidBody(rigidBody->GetRigidBody(), 1 << rigidBody->GetGroup(), rigidBody->GetGroupMask());
}

void PhysicsManager::RemoveRigidBody(RigidBody* rigidBody)
{
	mWorld->removeRigidBody(rigidBody->GetRigidBody());
}

void PhysicsManager::AddJoint(Joint* joint)
{
	if (joint->GetConstraint() == nullptr)
	{
		return;
	}

	mWorld->addConstraint(joint->GetConstraint());
}

void PhysicsManager::RemoveJoint(Joint* joint)
{
	if (joint->GetConstraint() == nullptr)
	{
		return;
	}

	mWorld->removeConstraint(joint->GetConstraint());
}

btDiscreteDynamicsWorld* PhysicsManager::GetDynamicsWorld()
{
	return mWorld.get();
}

void PhysicsManager::UpdateByThread()
{
	unsigned long prevTime = timeGetTime();

	while (true)
	{
		if (mStopFlag.load() == true)
		{
			break;
		}

		unsigned long currentTime = timeGetTime();
		unsigned long deltaTime = currentTime - prevTime;
		prevTime = currentTime;

		mWorld->stepSimulation(deltaTime * 0.001f, mMaxSubStepCount, mFixedTimeStep);
	}

	mEndFlag.store(true);
}






