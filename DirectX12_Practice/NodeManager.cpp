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

		_duration = std::max(_duration, _boneNodeByIdx[index]->GetMaxFrameNo());
	}

	for (int index = 0; index < _boneNodeByIdx.size(); index++)
	{
		BoneNode* currentBoneNode = _boneNodeByIdx[index];

		unsigned int parentBoneIndex = currentBoneNode->GetParentBoneIndex();
		if (parentBoneIndex == 65535 ||
			_boneNodeByIdx.size() <= parentBoneIndex || 
			parentBoneIndex < 0)
		{
			continue;
		}

		currentBoneNode->SetParentBoneNode(_boneNodeByIdx[currentBoneNode->GetParentBoneIndex()]);
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

void NodeManager::SortMotionKey()
{
	for (int index = 0; index < _boneNodeByIdx.size(); index++)
	{
		BoneNode* currentBoneNode = _boneNodeByIdx[index];
		currentBoneNode->SortAllKeys();
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
		curNode->UpdateLocalTransform();
	}

	for (BoneNode* curNode : _boneNodeByIdx)
	{
		curNode->UpdateGlobalTransform();
	}
}


