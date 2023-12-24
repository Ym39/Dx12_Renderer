#pragma once
#include<DirectXMath.h>
#include<vector>

#include "BoneNode.h"
#include "PmxFileData.h"

class IKSolver;
struct IKChain
{
	BoneNode* boneNode;
	bool enableAxisLimit;
	DirectX::XMFLOAT3 limitMin;
	DirectX::XMFLOAT3 limitMax;
	DirectX::XMFLOAT3 prevAngle;
	DirectX::XMFLOAT4 saveIKRotation;
	float planeModeAngle;

	IKChain(BoneNode* linkNode, bool axisLimit, const DirectX::XMFLOAT3& limitMinimum, const DirectX::XMFLOAT3 limitMaximum)
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
	IKSolver(BoneNode* node, BoneNode* targetNode, unsigned int iterationCount, float limitAngle):
	_ikNode(node),
	_targetNode(targetNode),
	_ikIterationCount(iterationCount),
	_ikLimitAngle(limitAngle),
	_enable(true)
	{
	}

	void Solve();

	void AddIKChain(BoneNode* linkNode, bool enableAxisLimit, const DirectX::XMFLOAT3& limitMin, const DirectX::XMFLOAT3 limitMax)
	{
		_ikChains.emplace_back(linkNode, enableAxisLimit, limitMin, limitMax);
	}

	const std::wstring& GetIKNodeName() const;

	bool GetEnable() const { return _enable; }
	void SetEnable(bool enable) { _enable = enable; }

	BoneNode* GetIKNode() const { return _ikNode; }
	BoneNode* GetTargetNode() const { return _targetNode; }

	const std::vector<IKChain>& GetIKChains() const { return _ikChains; }

	unsigned int GetIterationCount() const { return _ikIterationCount; }
	float GetLimitAngle() const { return _ikLimitAngle; }

private:
	void SolveCore(unsigned int iteration);

private:
	bool _enable;

	BoneNode* _ikNode;
	BoneNode* _targetNode;

	std::vector<IKChain> _ikChains;

	unsigned int _ikIterationCount;
	float _ikLimitAngle;
};

