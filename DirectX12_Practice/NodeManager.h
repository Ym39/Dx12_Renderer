#pragma once
#include<unordered_map>

#include "IKSolver.h"

class NodeManager
{
public:
	NodeManager() = default;

	void Init(const std::vector<PMXBone>& bones);
	void SortKey();

	BoneNode* GetBoneNodeByIndex(int index) const;
	BoneNode* GetBoneNodeByName(std::wstring& name) const;

	void UpdateAnimation(unsigned int frameNo);

private:
	std::unordered_map<std::wstring, BoneNode*> _boneNodeByName;
	std::vector<BoneNode*> _boneNodeByIdx;
	std::vector<BoneNode*> _sortedNodes;

	std::vector<IKSolver> _ikSolvers;

	unsigned int _duration = 0;
};

