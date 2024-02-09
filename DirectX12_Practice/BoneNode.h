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
	unsigned int GetIKTargetBoneIndex() const { return _ikTargetBoneIndex; }

	bool GetIKEnable() const { return _enableIK; }
	void SetIKEnable(bool enable) { _enableIK = enable; }

	IKSolver* GetIKSolver() const { return _ikSolver; }
	void SetIKSolver(IKSolver* ikSolver);

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
	void SetLocalTransform(const XMMATRIX& local) { _localTransform = local; }
	const XMMATRIX& GetGlobalTransform() const { return _globalTransform; }
	void SetGlobalTransform(const XMMATRIX& global) { _globalTransform = global; }

	void SetAnimateRotation(const XMMATRIX& rotation) { _animateRotation = rotation; }
	const XMMATRIX& GetAnimateRotation() const { return _animateRotation; }
	const XMFLOAT3& GetAnimatePosition() const { return _animatePosition; }

	void SetPosition(const XMFLOAT3& position) { _position = position; }
	const XMFLOAT3& GetPosition() const { return _position; }

	void SetIKRotation(const XMMATRIX& rotation) { _ikRotation = rotation; }
	const XMMATRIX& GetIKRotation() const { return _ikRotation; }

	void SetMorphPosition(const XMFLOAT3& position) { _morphPosition = position; }
	void SetMorphRotation(const XMMATRIX& rotation) { _morphRotation = rotation; }

	void AddMotionKey(unsigned int& frameNo, XMFLOAT4& quaternion, XMFLOAT3& offset, XMFLOAT2& p1, XMFLOAT2& p2);
	void AddIKkey(unsigned int& frameNo, bool& enable);
	void SortAllKeys();

	void SetDeformAfterPhysics(bool deformAfterPhysics) { _deformAfterPhysics = deformAfterPhysics; }
	bool GetDeformAfterPhysics() const { return _deformAfterPhysics; }

	void SetEnableAppendRotate(bool enable) { _isAppendRotate = enable; }
	void SetEnableAppendTranslate(bool enable) { _isAppendTranslate = enable; }
	void SetEnableAppendLocal(bool enable) { _isAppendLocal = enable; }
	void SetAppendWeight(float weight) { _appendWeight = weight; }
	float GetAppendWeight() const { return _appendWeight; }
	void SetAppendBoneNode(BoneNode* node) { _appendBoneNode = node; }
	BoneNode* GetAppendBoneNode() const { return _appendBoneNode; }
	const XMMATRIX& GetAppendRotation() const { return _appendRotation; }
	const XMFLOAT3& GetAppendTranslate() const { return _appendTranslate; }

	unsigned int GetMaxFrameNo() const;

	void UpdateAppendTransform();

	void UpdateLocalTransform();
	void UpdateGlobalTransform();
	void UpdateGlobalTransformNotUpdateChildren();
	void UpdateChildTransform();

	void AnimateMotion(unsigned int frameNo);
	void AnimateIK(unsigned int frameNo);

private:
	float GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n);

private:
	unsigned int _boneIndex;
	std::wstring _name;
	XMFLOAT3 _position;
	unsigned int _parentBoneIndex = -1;
	unsigned int _deformDepth;
	bool _deformAfterPhysics;
	PMXBoneFlags _boneFlag;
	unsigned int _appendBoneIndex;
	unsigned int _ikTargetBoneIndex;
	unsigned int _ikIterationCount;
	float _ikLimit;
	bool _enableIK = false;

	XMFLOAT3 _animatePosition;
	XMMATRIX _animateRotation;

	XMFLOAT3 _morphPosition;
	XMMATRIX _morphRotation;

	XMMATRIX _ikRotation;

	XMFLOAT3 _appendTranslate;
	XMMATRIX _appendRotation;

	XMMATRIX _inverseInitTransform;
	XMMATRIX _localTransform;
	XMMATRIX _globalTransform;

	BoneNode* _parentBoneNode = nullptr;
	std::vector<BoneNode*> _childrenNodes;

	bool _isAppendRotate = false;
	bool _isAppendTranslate = false;
	bool _isAppendLocal = false;
	float _appendWeight = 0.f;
	BoneNode* _appendBoneNode = nullptr;

	std::vector<VMDKey> _motionKeys;
	std::vector<VMDIKkey> _ikKeys;

	IKSolver* _ikSolver = nullptr;

	int _prevAnimationFrameNo = -1;
	int _prevIKFrameNo = -1;

	std::reverse_iterator<std::vector<VMDKey>::iterator> _currentAnimationFrameIt;
	std::vector<VMDKey>::iterator _nextAnimationFrameIt;

	std::reverse_iterator<std::vector<VMDIKkey>::iterator> _currentIKFrameIt;
};

