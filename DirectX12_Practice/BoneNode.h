#pragma once
#include <string>
#include <DirectXMath.h>

#include "PmxFileData.h"

using namespace DirectX;

class IKSolver;

struct VMDKey
{
	unsigned int frameNo;
	XMVECTOR quaternion;
	XMFLOAT3 offset;
	XMFLOAT2 p1;
	XMFLOAT2 p2;

	VMDKey(unsigned int frameNo, XMVECTOR& quaternion, XMFLOAT3& offset, XMFLOAT2& p1, XMFLOAT2& p2) :
		frameNo(frameNo),
		quaternion(quaternion),
		offset(offset),
		p1(p1),
		p2(p2)
	{}
};

struct VMDIKkey
{
	unsigned int frameNo;
	bool enable;

	VMDIKkey(unsigned int frameNo, bool enable) :
		frameNo(frameNo),
		enable(enable)
	{}
};

class BoneNode
{
public:
	BoneNode(unsigned int index, const PMXBone& pmxBone);

	unsigned int GetBoneIndex() const { return _boneIndex; }
	const std::wstring& GetName() const { return _name; }
	unsigned int GetParentBoneIndex() const { return _parentBoneIndex; }
	unsigned int GetDeformDepth() const { return _deformDepth; }
	unsigned int GetAppendBoneIndex() const { return _appendBoneIndex; }
	float GetAppendWeight() const { return _appendWeight; }
	unsigned int GetIKTargetBoneIndex() const { return _ikTargetBoneIndex; }

	bool GetIKEnable() const { return _enableIK; }
	void SetIKEnable(bool enable) { _enableIK = enable; }

	IKSolver* GetIKSolver() const { return _ikSolver; }
	void SetIKSolver(IKSolver* ikSolver) { _ikSolver = ikSolver; }

	void SetParentBoneNode(BoneNode* parentNode)
	{
		_parentBoneNode = parentNode;
		_parentBoneNode->AddChildBoneNode(this);
	}
	const BoneNode* GetParentBoneNode() const { return _parentBoneNode; }

	void AddChildBoneNode(BoneNode* childNode) { _childrenNodes.push_back(childNode); }
	const std::vector<BoneNode*>& GetChildrenNodes() const { return _childrenNodes; }

	const XMMATRIX& GetInitInverseTransform() const { return _inverseInitTransform; }
	const XMMATRIX& GetLocalTransform() const { return _localTransform; }
	const XMMATRIX& GetGlobalTransform() const { return _globalTransform; }

	void SetPosition(const XMFLOAT3& position) { _position = position; }
	const XMFLOAT3& GetPosition() const { return _position; }

	void AddMotionKey(unsigned int& frameNo, XMFLOAT4& quaternion, XMFLOAT3& offset, XMFLOAT2& p1, XMFLOAT2& p2);
	void AddIKkey(unsigned int& frameNo, bool& enable);
	void SortAllKeys();

	unsigned int GetMaxFrameNo() const;

	void UpdateLocalTransform();
	void UpdateGlobalTransform();

	void AnimateMotion(unsigned int frameNo);

private:
	float GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n);

private:
	unsigned int _boneIndex;
	std::wstring _name;
	XMFLOAT3 _position;
	unsigned int _parentBoneIndex;
	unsigned int _deformDepth;
	PMXBoneFlags _boneFlag;
	unsigned int _appendBoneIndex;
	float _appendWeight;
	unsigned int _ikTargetBoneIndex;
	unsigned int _ikIterationCount;
	float _ikLimit;
	bool _enableIK = false;

	XMFLOAT3 _animatePosition;
	XMMATRIX _animateRotation;

	XMMATRIX _inverseInitTransform;
	XMMATRIX _localTransform;
	XMMATRIX _globalTransform;

	BoneNode* _parentBoneNode = nullptr;
	std::vector<BoneNode*> _childrenNodes;

	std::vector<VMDKey> _motionKeys;
	std::vector<VMDIKkey> _ikKeys;

	IKSolver* _ikSolver = nullptr;
};

