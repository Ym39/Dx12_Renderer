#include "NodeManager.h"
#include <algorithm>

void NodeManager::Init(const std::vector<PMXBone>& bones)
{
	_boneNodeByIdx.resize(bones.size());
	_sortedNodes.resize(bones.size());

	for (int index = 0; index < bones.size(); index++)
	{
		const PMXBone& currentBoneData = bones[index];
		_boneNodeByIdx[index] = new BoneNode(index, currentBoneData);
		_boneNodeByName[_boneNodeByIdx[index]->GetName()] = _boneNodeByIdx[index];
		_sortedNodes[index] = _boneNodeByIdx[index];
	}

	for (int index = 0; index < _boneNodeByIdx.size(); index++)
	{
		//Parent Bone Set
		BoneNode* currentBoneNode = _boneNodeByIdx[index];

		unsigned int parentBoneIndex = currentBoneNode->GetParentBoneIndex();
		if (parentBoneIndex != 65535 && _boneNodeByIdx.size() > parentBoneIndex)
		{
			currentBoneNode->SetParentBoneNode(_boneNodeByIdx[currentBoneNode->GetParentBoneIndex()]);
		}

		const PMXBone& currentPmxBone = bones[index];

		//Append Bone Setting
		bool appendRotate = (uint16_t)currentPmxBone.boneFlag & (uint16_t)PMXBoneFlags::AppendRotate;
		bool appendTranslate = (uint16_t)currentPmxBone.boneFlag & (uint16_t)PMXBoneFlags::AppendTranslate;
		currentBoneNode->SetEnableAppendRotate(appendRotate);
		currentBoneNode->SetEnableAppendTranslate(appendTranslate);
		if ((appendRotate || appendTranslate) && currentPmxBone.appendBoneIndex < _boneNodeByIdx.size())
		{
			if (index > currentPmxBone.appendBoneIndex)
			{
				bool appendLocal = (uint16_t)currentPmxBone.boneFlag & (uint16_t)PMXBoneFlags::AppendLocal;
				BoneNode* appendBoneNode = _boneNodeByIdx[currentPmxBone.appendBoneIndex];
				currentBoneNode->SetEnableAppendLocal(appendLocal);
				currentBoneNode->SetAppendBoneNode(appendBoneNode);
				currentBoneNode->SetAppendWeight(currentPmxBone.appendWeight);
			}
		}

		//IK Solver Setting
		if (((uint16_t)currentPmxBone.boneFlag & (uint16_t)PMXBoneFlags::IK) && currentPmxBone.ikTargetBoneIndex < _boneNodeByIdx.size())
		{
			BoneNode* targetNode = _boneNodeByIdx[currentPmxBone.ikTargetBoneIndex];
			unsigned int iterationCount = currentPmxBone.ikIterationCount;
			float limitAngle = currentPmxBone.ikLimit;

			_ikSolvers.push_back(new IKSolver(currentBoneNode, targetNode, iterationCount, limitAngle));

			IKSolver* solver = _ikSolvers[_ikSolvers.size() - 1];

			for (const PMXIKLink& ikLink : currentPmxBone.ikLinks)
			{
				if (ikLink.ikBoneIndex < 0 || ikLink.ikBoneIndex >= _boneNodeByIdx.size())
				{
					continue;
				}

				BoneNode* linkNode = _boneNodeByIdx[ikLink.ikBoneIndex];
				if (ikLink.enableLimit == true)
				{
					solver->AddIKChain(linkNode, ikLink.enableLimit, ikLink.limitMin, ikLink.limitMax);
				}
				else
				{
					solver->AddIKChain(
						linkNode,
						ikLink.enableLimit,
						XMFLOAT3(0.5f, 0.f, 0.f),
						XMFLOAT3(180.f, 0.f, 0.f));
				}
				linkNode->SetIKEnable(true);
			}
			currentBoneNode->SetIKSolver(solver);
		}
	}

	for (int index = 0; index < _boneNodeByIdx.size(); index++)
	{
		BoneNode* currentBoneNode = _boneNodeByIdx[index];

		if (currentBoneNode->GetParentBoneNode() == nullptr)
		{
			continue;
		}

		XMVECTOR pos = XMLoadFloat3(&bones[currentBoneNode->GetBoneIndex()].position);
		XMVECTOR parentPos = XMLoadFloat3(&bones[currentBoneNode->GetParentBoneIndex()].position);

		XMFLOAT3 resultPos;
		XMStoreFloat3(&resultPos, pos - parentPos);

		currentBoneNode->SetPosition(resultPos);
	}

	std::stable_sort(_sortedNodes.begin(), _sortedNodes.end(), 
		[](const BoneNode* left, const BoneNode * right)
	{
			return left->GetDeformDepth() < right->GetDeformDepth();
	});
}

void NodeManager::SortKey()
{
	for (int index = 0; index < _boneNodeByIdx.size(); index++)
	{
		BoneNode* currentBoneNode = _boneNodeByIdx[index];
		currentBoneNode->SortAllKeys();
		_duration = std::max(_duration, currentBoneNode->GetMaxFrameNo());
	}
}

BoneNode* NodeManager::GetBoneNodeByIndex(int index) const
{
	if (_boneNodeByIdx.size() <= index)
	{
		return nullptr;
	}

	return _boneNodeByIdx[index];
}

BoneNode* NodeManager::GetBoneNodeByName(std::wstring& name) const
{
	auto it = _boneNodeByName.find(name);
	if (it == _boneNodeByName.end())
	{
		return nullptr;
	}

	return it->second;
}

void NodeManager::UpdateAnimation(unsigned int frameNo)
{
	for (BoneNode* curNode : _boneNodeByIdx)
	{
		curNode->AnimateMotion(frameNo);
		curNode->AnimateIK(frameNo);
		curNode->UpdateLocalTransform();
	}

	if (_boneNodeByIdx.size() > 0)
	{
		_boneNodeByIdx[0]->UpdateGlobalTransform();
	}

	for (BoneNode* curNode : _sortedNodes)
	{
		if (curNode->GetAppendBoneNode() != nullptr)
		{
			curNode->UpdateAppendTransform();
			curNode->UpdateGlobalTransform();
		}

		IKSolver* curSolver = curNode->GetIKSolver();
		if (curSolver != nullptr)
		{
			curSolver->Solve();
			curNode->UpdateGlobalTransform();
		}
	}
}


