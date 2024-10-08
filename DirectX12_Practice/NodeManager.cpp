#include "NodeManager.h"
#include <algorithm>

NodeManager::NodeManager()
{
}

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

		bool deformAfterPhysics = (uint16_t)currentPmxBone.boneFlag & (uint16_t)PMXBoneFlags::DeformAfterPhysics;
		currentBoneNode->SetDeformAfterPhysics(deformAfterPhysics);

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

	OrderingLocalUpdate(false);
	OrderingLocalUpdate(true);
	OrderingGlobalUpdate(false);
	OrderingGlobalUpdate(true);
	OrderingAppendOrIKUpdate(false);
	OrderingAppendOrIKUpdate(true);

	InitParallelAnimateEvaluate();
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

void NodeManager::BeforeUpdateAnimation()
{
	//for (BoneNode* curNode : _boneNodeByIdx)
	//{
	//	curNode->SetMorphPosition(XMFLOAT3(0.f, 0.f, 0.f));
	//	curNode->SetMorphRotation(XMMatrixIdentity());
	//}
}

void NodeManager::EvaluateAnimation(unsigned frameNo)
{
	const int futureCount = _parallelEvaluateFutures.size();

	for (int i = 0; i < futureCount; i++)
	{
		const AnimateEvaluateRange& currentRange = _animateEvaluateRanges[i];
		_parallelEvaluateFutures[i] = std::async(std::launch::async, [this, frameNo, currentRange]()
			{
				for (int i = currentRange.startIndex; i < currentRange.startIndex + currentRange.vertexCount; ++i)
				{
					BoneNode* curNode = _boneNodeByIdx[i];
					curNode->AnimateMotion(frameNo);
					curNode->AnimateIK(frameNo);
				}
			});
	}

	for (const std::future<void>& future : _parallelEvaluateFutures)
	{
		future.wait();
	}

	//for (BoneNode* curNode : _boneNodeByIdx)
	//{
	//	curNode->AnimateMotion(frameNo);
	//	curNode->AnimateIK(frameNo);
	//}
}

void NodeManager::InitAnimation()
{
	for (BoneNode* curNode : _sortedNodes)
	{
		curNode->UpdateLocalTransform();
	}

	for (BoneNode* curNode : _sortedNodes)
	{

		if (curNode->GetParentBoneNode() != nullptr)
		{
			continue;
		}

		curNode->UpdateGlobalTransform();
	}
}

void NodeManager::UpdateAnimation()
{
	//for (BoneNode* curNode : _sortedNodes)
	//{
	//	if (curNode->GetDeformAfterPhysics() == true)
	//	{
	//		continue;
	//	}

	//	curNode->UpdateLocalTransform();
	//}

	//for (BoneNode* curNode : _sortedNodes)
	//{
	//	if (curNode->GetDeformAfterPhysics() == true)
	//	{
	//		continue;
	//	}

	//	if (curNode->GetParentBoneNode() != nullptr)
	//	{
	//		continue;
	//	}

	//	curNode->UpdateGlobalTransform();
	//}

	for (int index : _beforePhysicsLocalUpdateOrder)
	{
		_boneNodeByIdx[index]->UpdateLocalTransform();
	}

	for (int index : _beforePhysicsGlobalUpdateOrder)
	{
		_boneNodeByIdx[index]->UpdateGlobalTransformNotUpdateChildren();
	}

	//for (BoneNode* curNode : _sortedNodes)
	//{
	//	if (curNode->GetDeformAfterPhysics() == true)
	//	{
	//		continue;
	//	}

	//	if (curNode->GetAppendBoneNode() != nullptr)
	//	{
	//		curNode->UpdateAppendTransform();
	//		curNode->UpdateGlobalTransform();
	//	}

	//	IKSolver* curSolver = curNode->GetIKSolver();
	//	if (curSolver != nullptr)
	//	{
	//		curSolver->Solve();
	//		curNode->UpdateGlobalTransform();
	//	}
	//}

	for (int index : _beforePhysicsAppendOrIKUpdateOrder)
	{
		BoneNode* curNode = _boneNodeByIdx[index];

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

void NodeManager::UpdateAnimationAfterPhysics()
{
	//for (BoneNode* curNode : _sortedNodes)
	//{
	//	if (curNode->GetDeformAfterPhysics() == false)
	//	{
	//		continue;
	//	}

	//	curNode->UpdateLocalTransform();
	//}

	//for (BoneNode* curNode : _sortedNodes)
	//{
	//	if (curNode->GetDeformAfterPhysics() == false)
	//	{
	//		continue;
	//	}

	//	if (curNode->GetParentBoneNode() != nullptr)
	//	{
	//		continue;
	//	}

	//	curNode->UpdateGlobalTransform();
	//}

	for (int index : _afterPhysicsLocalUpdateOrder)
	{
		_boneNodeByIdx[index]->UpdateLocalTransform();
	}

	for (int index : _afterPhysicsGlobalUpdateOrder)
	{
		_boneNodeByIdx[index]->UpdateGlobalTransformNotUpdateChildren();
	}

	//for (BoneNode* curNode : _sortedNodes)
	//{
	//	if (curNode->GetDeformAfterPhysics() == false)
	//	{
	//		continue;
	//	}

	//	if (curNode->GetAppendBoneNode() != nullptr)
	//	{
	//		curNode->UpdateAppendTransform();
	//		curNode->UpdateGlobalTransform();
	//	}

	//	IKSolver* curSolver = curNode->GetIKSolver();
	//	if (curSolver != nullptr)
	//	{
	//		curSolver->Solve();
	//		curNode->UpdateGlobalTransform();
	//	}
	//}

	for (int index : _afterPhysicsAppendOrIKUpdateOrder)
	{
		BoneNode* curNode = _boneNodeByIdx[index];

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

void NodeManager::Dispose()
{
	for (BoneNode* boneNode : _sortedNodes)
	{
		delete boneNode;
		boneNode = nullptr;
	}

	for (IKSolver* ikSolver : _ikSolvers)
	{
		delete ikSolver;
		ikSolver = nullptr;
	}
}

void NodeManager::OrderingLocalUpdate(bool afterPhysics)
{
	for (BoneNode* curNode : _sortedNodes)
	{
		if (curNode->GetDeformAfterPhysics() == !afterPhysics)
		{
			continue;
		}

		_beforePhysicsLocalUpdateOrder.push_back(curNode->GetBoneIndex());
	}
}

void NodeManager::OrderingGlobalUpdate(bool afterPhysics)
{
	for (BoneNode* curNode : _sortedNodes)
	{
		if (curNode->GetDeformAfterPhysics() == !afterPhysics)
		{
			continue;
		}

		if (curNode->GetParentBoneNode() != nullptr)
		{
			continue;
		}

		AddGlobalOrder(curNode, afterPhysics);
	}
}

void NodeManager::OrderingAppendOrIKUpdate(bool afterPhysics)
{
	for (BoneNode* curNode : _sortedNodes)
	{
		if (curNode->GetDeformAfterPhysics() == !afterPhysics)
		{
			continue;
		}

		if (curNode->GetAppendBoneNode() != nullptr)
		{
			_beforePhysicsAppendOrIKUpdateOrder.push_back(curNode->GetBoneIndex());
			continue;
		}

		IKSolver* curSolver = curNode->GetIKSolver();
		if (curSolver != nullptr)
		{
			_beforePhysicsAppendOrIKUpdateOrder.push_back(curNode->GetBoneIndex());
		}
	}
}

void NodeManager::AddGlobalOrder(BoneNode* node, bool afterPhysics)
{
	if (afterPhysics == false)
	{
		_beforePhysicsGlobalUpdateOrder.push_back(node->GetBoneIndex());
	}
	else
	{
		_afterPhysicsGlobalUpdateOrder.push_back(node->GetBoneIndex());
	}

	const auto childNodes = node->GetChildrenNodes();

	for (BoneNode* node : childNodes)
	{
		AddGlobalOrder(node, afterPhysics);
	}
}

void NodeManager::InitParallelAnimateEvaluate()
{
	unsigned int threadCount = (std::thread::hardware_concurrency() * 2) + 1;
	unsigned int divNum = threadCount - 1;

	_animateEvaluateRanges.resize(threadCount);
	_parallelEvaluateFutures.resize(threadCount);

	unsigned int divVertexCount = _boneNodeByIdx.size() / divNum;
	unsigned int remainder = _boneNodeByIdx.size() % divNum;

	int startIndex = 0;
	for (int i = 0; i < _animateEvaluateRanges.size() - 1; i++)
	{
		_animateEvaluateRanges[i].startIndex = startIndex;
		_animateEvaluateRanges[i].vertexCount = divVertexCount;

		startIndex += _animateEvaluateRanges[i].vertexCount;
	}

	_animateEvaluateRanges[_animateEvaluateRanges.size() - 1].startIndex = startIndex;
	_animateEvaluateRanges[_animateEvaluateRanges.size() - 1].vertexCount = remainder;
}


