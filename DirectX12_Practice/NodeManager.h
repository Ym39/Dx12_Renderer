#pragma once
#include <unordered_map>
#include <thread>
#include <future>

#include "IKSolver.h"

struct AnimateEvaluateRange
{
	unsigned int startIndex;
	unsigned int vertexCount;
};

class NodeManager
{
public:
	NodeManager();

	void Init(const std::vector<PMXBone>& bones);
	void SortKey();

	BoneNode* GetBoneNodeByIndex(int index) const;
	BoneNode* GetBoneNodeByName(std::wstring& name) const;

	const std::vector<BoneNode*>& GetAllNodes() const { return _boneNodeByIdx; }

	void BeforeUpdateAnimation();

	void EvaluateAnimation(unsigned int frameNo);
	void InitAnimation();
	void UpdateAnimation();
	void UpdateAnimationAfterPhysics();

	void Dispose();

private:
	void OrderingLocalUpdate(bool afterPhysics);
	void OrderingGlobalUpdate(bool afterPhysics);
	void OrderingAppendOrIKUpdate(bool afterPhysics);
	void AddGlobalOrder(BoneNode* node, bool afterPhysics);
	void InitParallelAnimateEvaluate();

private:
	std::unordered_map<std::wstring, BoneNode*> _boneNodeByName;
	std::vector<BoneNode*> _boneNodeByIdx;
	std::vector<BoneNode*> _sortedNodes;

	std::vector<IKSolver*> _ikSolvers;

	std::vector<AnimateEvaluateRange> _animateEvaluateRanges;
	std::vector<std::future<void>> _parallelEvaluateFutures;

	std::vector<int> _beforePhysicsLocalUpdateOrder;
	std::vector<int> _beforePhysicsGlobalUpdateOrder;
	std::vector<int> _beforePhysicsAppendOrIKUpdateOrder;

	std::vector<int> _afterPhysicsLocalUpdateOrder;
	std::vector<int> _afterPhysicsGlobalUpdateOrder;
	std::vector<int> _afterPhysicsAppendOrIKUpdateOrder;

	unsigned int _duration = 0;
};

