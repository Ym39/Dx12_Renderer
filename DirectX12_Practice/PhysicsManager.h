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
	static std::unique_ptr<btDiscreteDynamicsWorld> _world;
	static std::unique_ptr<btBroadphaseInterface> _broadPhase;
	static std::unique_ptr<btDefaultCollisionConfiguration> _collisionConfig;
	static std::unique_ptr<btCollisionDispatcher> _dispatcher;
	static std::unique_ptr<btSequentialImpulseConstraintSolver> _solver;
	static std::unique_ptr<btCollisionShape> _groundShape;
	static std::unique_ptr<btMotionState> _groundMS;
	static std::unique_ptr<btRigidBody> _groundRB;
	static std::unique_ptr<btOverlapFilterCallback> _filterCB;

	static float _fixedTimeStep;
	static int _maxSubStepCount;

	static std::thread _physicsUpdateThread;
	static bool _threadFlag;

	static std::atomic<bool> _stopFlag;
	static std::atomic<bool> _endFlag;
};

