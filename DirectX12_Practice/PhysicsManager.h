#pragma once
#include <memory>
#include <thread>
#include <atomic>

#include "LinearMath/btAlignedObjectArray.h"

class btSequentialImpulseConstraintSolver;
class RigidBody;
class Joint;

class btRigidBody;
class btCollisionShape;
class btTypedConstraint;
class btDiscreteDynamicsWorld;
class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btCollisionDispatcherMt;
class btMotionState;
class btITaskScheduler;
struct btOverlapFilterCallback;
class MySequentialImpulseConstraintSolverMt;

class PhysicsManager
{
public:
	PhysicsManager();
	~PhysicsManager();

	PhysicsManager(const PhysicsManager& other) = delete;
	PhysicsManager& operator = (const PhysicsManager& other) = delete;

	static bool Create();
	static void Destroy();

	static void SetMaxSubStepCount(int numSteps);
	static int GetMaxSubStepCount();
	static void SetFixedTimeStep(float fixedTimeStep);
	static float GetFixedTimeStep();

	static void ActivePhysics(bool active);
	static void ForceUpdatePhysics();

	static void AddRigidBody(RigidBody* rigidBody);
	static void RemoveRigidBody(RigidBody* rigidBody);
	static void AddJoint(Joint* joint);
	static void RemoveJoint(Joint* joint);

	static btDiscreteDynamicsWorld* GetDynamicsWorld();

private:
	static void UpdateByThread();

private:
	static std::unique_ptr<btDiscreteDynamicsWorld> mWorld;
	static std::unique_ptr<btBroadphaseInterface> mBroadPhase;
	static std::unique_ptr<btDefaultCollisionConfiguration> mCollisionConfig;
	static std::unique_ptr<btCollisionDispatcher> mDispatcher;
	static std::unique_ptr<btSequentialImpulseConstraintSolver> mSolver;
	static std::unique_ptr<btCollisionShape> mGroundShape;
	static std::unique_ptr<btMotionState> mGroundMS;
	static std::unique_ptr<btRigidBody> mGroundRB;
	static std::unique_ptr<btOverlapFilterCallback> mFilterCB;

	static float mFixedTimeStep;
	static int mMaxSubStepCount;

	static std::thread mPhysicsUpdateThread;
	static bool mThreadFlag;

	static std::atomic<bool> mStopFlag;
	static std::atomic<bool> mEndFlag;
};

