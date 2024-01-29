#pragma once
#include<unordered_map>

#include "IKSolver.h"

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
	std::unordered_map<std::wstring, BoneNode*> _boneNodeByName;
	std::vector<BoneNode*> _boneNodeByIdx;
	std::vector<BoneNode*> _sortedNodes;

	std::vector<IKSolver*> _ikSolvers;

	unsigned int _duration = 0;
};

