#pragma once
#include<DirectXMath.h>
#include<vector>

#include "PmxFileData.h"

class IKSolver;
struct BoneNodeLegacy
{
	unsigned int boneIndex;

	std::wstring name;
	std::string englishName;

	DirectX::XMFLOAT3 position;
	unsigned int parentBoneIndex;
	unsigned int deformDepth;

	PMXBoneFlags boneFlag;

	DirectX::XMFLOAT3 positionOffset;
	unsigned int linkBoneIndex;

	unsigned int appendBoneIndex;
	float appendWeight;

	DirectX::XMFLOAT3 fixedAxis;
	DirectX::XMFLOAT3 localXAxis;
	DirectX::XMFLOAT3 localZAxis;

	unsigned int keyValue;

	unsigned int ikTargetBoneIndex;
	unsigned int ikIterationCount;
	unsigned int ikLimit;

	BoneNodeLegacy* parentNode;
	std::vector<BoneNodeLegacy*> childrenNode;

	DirectX::XMFLOAT3 animatePosition;
	DirectX::XMMATRIX animateRotation;

	IKSolver* ikSolver;
	bool enableIK;

	const DirectX::XMMATRIX& GetLocalTransform()
	{

		return animateRotation * DirectX::XMMatrixTranslationFromVector(DirectX::XMVectorAdd(XMLoadFloat3(&position), XMLoadFloat3(&animatePosition)));
	}
};

struct IKChain
{
	BoneNodeLegacy* boneNode;
	bool enableAxisLimit;
	DirectX::XMFLOAT3 limitMin;
	DirectX::XMFLOAT3 limitMax;
	DirectX::XMFLOAT3 prevAngle;
	DirectX::XMFLOAT4 saveIKRotation;
	float planeModeAngle;

	IKChain(BoneNodeLegacy* linkNode, bool axisLimit, const DirectX::XMFLOAT3& limitMinimum, const DirectX::XMFLOAT3 limitMaximum)
	{
		boneNode = linkNode;
		enableAxisLimit = axisLimit;
		limitMin = limitMinimum;
		limitMax = limitMaximum;
		saveIKRotation = DirectX::XMFLOAT4(0.f, 0.f, 0.f, 0.f);
	}
};

class IKSolver
{
public:
	IKSolver(BoneNodeLegacy* node, BoneNodeLegacy* targetNode, unsigned int iterationCount, float limitAngle):
	_ikNode(node),
	_targetNode(targetNode),
	_ikIterationCount(iterationCount),
	_ikLimitAngle(limitAngle),
	_enable(true)
	{
	}

	void Solve();

	void AddIKChain(BoneNodeLegacy* linkNode, bool enableAxisLimit, const DirectX::XMFLOAT3& limitMin, const DirectX::XMFLOAT3 limitMax)
	{
		_ikChains.emplace_back(linkNode, enableAxisLimit, limitMin, limitMax);
	}

	const std::wstring& GetIKNodeName() const
	{
		if (_ikNode == nullptr)
		{
			return L"";
		}
		else
		{
			_ikNode->name;
		}
	}

	bool GetEnable() const { return _enable; }
	void SetEnable(bool enable) { _enable = enable; }

	BoneNodeLegacy* GetIKNode() const { return _ikNode; }
	BoneNodeLegacy* GetTargetNode() const { return _targetNode; }

	const std::vector<IKChain>& GetIKChains() const { return _ikChains; }

	unsigned int GetIterationCount() const { return _ikIterationCount; }
	float GetLimitAngle() const { return _ikLimitAngle; }

private:
	void SolveCore(unsigned int iteration);

private:
	bool _enable;

	BoneNodeLegacy* _ikNode;
	BoneNodeLegacy* _targetNode;

	std::vector<IKChain> _ikChains;

	unsigned int _ikIterationCount;
	float _ikLimitAngle;
};

