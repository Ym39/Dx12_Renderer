#include "BoneNode.h"

#include <algorithm>

#include "IKSolver.h"

constexpr float epsilon = 0.0005f;

BoneNode::BoneNode(unsigned int index, const PMXBone& pmxBone) :
_boneIndex(index),
_name(pmxBone.name),
_position(pmxBone.position),
_parentBoneIndex(pmxBone.parentBoneIndex),
_deformDepth(pmxBone.deformDepth),
_boneFlag(pmxBone.boneFlag),
_appendBoneIndex(pmxBone.appendBoneIndex),
_ikTargetBoneIndex(pmxBone.ikTargetBoneIndex),
_ikIterationCount(pmxBone.ikIterationCount),
_ikLimit(pmxBone.ikLimit),
_animatePosition(XMFLOAT3(0.f, 0.f, 0.f)),
_animateRotation(XMMatrixIdentity()),
_inverseInitTransform(XMMatrixTranslation(-pmxBone.position.x, -pmxBone.position.y, -pmxBone.position.z)),
_localTransform(XMMatrixIdentity()),
_globalTransform(XMMatrixIdentity()),
_ikSolver(nullptr),
_appendTranslate(XMFLOAT3(0.f, 0.f, 0.f)),
_appendRotation(XMMatrixIdentity()),
_morphPosition(XMFLOAT3(0.f, 0.f, 0.f)),
_morphRotation(XMMatrixIdentity()),
_ikRotation(XMMatrixIdentity())
{
}

void BoneNode::SetIKSolver(IKSolver* ikSolver)
{
	_ikSolver = ikSolver;
}


void BoneNode::AddMotionKey(unsigned& frameNo, XMFLOAT4& quaternion, XMFLOAT3& offset, XMFLOAT2& p1, XMFLOAT2& p2)
{
	_motionKeys.emplace_back(frameNo, XMLoadFloat4(&quaternion), offset, p1, p2);
}

void BoneNode::AddIKkey(unsigned int& frameNo, bool& enable)
{
	_ikKeys.emplace_back(frameNo, enable);
}

void BoneNode::SortAllKeys()
{
	std::sort(_motionKeys.begin(), _motionKeys.end(),
		[](const VMDKey& left, const VMDKey& right)
		{
			return left.frameNo <= right.frameNo;
		});

	std::sort(_ikKeys.begin(), _ikKeys.end(),
		[](const VMDIKkey& left, const VMDIKkey& right)
		{
			return left.frameNo <= right.frameNo;
		});
}

unsigned int BoneNode::GetMaxFrameNo() const
{
	if (_motionKeys.size() <= 0)
	{
		return 0;
	}

	return _motionKeys[_motionKeys.size() - 1].frameNo;
}

void BoneNode::UpdateAppendTransform()
{
	if (_appendBoneNode == nullptr)
	{
		return;
	}

	XMMATRIX appendRotation;
	if (_isAppendRotate == true)
	{
		if (_isAppendLocal == true)
		{
			appendRotation = _appendBoneNode->GetAnimateRotation();
		}
		else
		{
			if (_appendBoneNode->GetAppendBoneNode() == nullptr)
			{
				appendRotation = _appendBoneNode->GetAnimateRotation();
			}
			else
			{
				appendRotation = _appendBoneNode->GetAppendRotation();
			}
		}

		if (_appendBoneNode->GetIKEnable() == true)
		{
			appendRotation = appendRotation * _appendBoneNode->GetIKRotation();
		}

		XMVECTOR appendRotationQuaternion = XMQuaternionRotationMatrix(appendRotation);
		appendRotationQuaternion = XMQuaternionSlerp(XMQuaternionIdentity(), appendRotationQuaternion, _appendWeight);

		_appendRotation = XMMatrixRotationQuaternion(appendRotationQuaternion);
	}

	XMVECTOR appendTranslate = XMVectorZero();
	if (_isAppendTranslate == true)
	{
		if (_isAppendLocal == true)
		{
			appendTranslate = XMLoadFloat3(&_appendBoneNode->GetAnimatePosition());
		}
		else
		{
			if (_appendBoneNode->GetAppendBoneNode() == nullptr)
			{
				appendTranslate = XMLoadFloat3(&_appendBoneNode->GetAnimatePosition());
			}
			else
			{
				appendTranslate = XMLoadFloat3(&_appendBoneNode->GetAppendTranslate());
			}
		}

		XMStoreFloat3(&_appendTranslate, appendTranslate);
	}

	UpdateLocalTransform();
}

void BoneNode::UpdateLocalTransform()
{
	XMMATRIX scale = XMMatrixIdentity();

	XMMATRIX rotation = _animateRotation;
	if (_enableIK == true)
	{
		rotation = rotation * _ikRotation;
	}

	if (_isAppendRotate == true)
	{
		rotation = rotation * _appendRotation;
	}

	XMVECTOR t = XMLoadFloat3(&_animatePosition) + XMLoadFloat3(&_position) + XMLoadFloat3(&_morphPosition);
	if (_isAppendTranslate == true)
	{
		t += XMLoadFloat3(&_appendTranslate);
	}

	XMMATRIX translate = XMMatrixTranslationFromVector(t);

	_localTransform = scale * rotation * translate;
}

void BoneNode::UpdateGlobalTransform()
{
	if (_parentBoneNode == nullptr)
	{
		_globalTransform = _localTransform;
	}
	else
	{
		_globalTransform = _localTransform * _parentBoneNode->GetGlobalTransform();
	}

	for (BoneNode* child : _childrenNodes)
	{
		child->UpdateGlobalTransform();
	}
}

void BoneNode::UpdateGlobalTransformNotUpdateChildren()
{
	if (_parentBoneNode == nullptr)
	{
		_globalTransform = _localTransform;
	}
	else
	{
		_globalTransform = _localTransform * _parentBoneNode->GetGlobalTransform();
	}
}

void BoneNode::UpdateChildTransform()
{
	for (BoneNode* child : _childrenNodes)
	{
		child->UpdateGlobalTransform();
	}
}

void BoneNode::AnimateMotion(unsigned int frameNo)
{
	_animateRotation = XMMatrixIdentity();
	_animatePosition = XMFLOAT3(0.f, 0.f, 0.f);

	if (_motionKeys.size() <= 0)
	{
		return;
	}

	if (_prevAnimationFrameNo != frameNo)
	{
		_currentAnimationFrameIt = std::find_if(_motionKeys.rbegin(), _motionKeys.rend(),
			[frameNo](const VMDKey& key)
			{
				return key.frameNo <= frameNo;
			});

		_nextAnimationFrameIt = _currentAnimationFrameIt.base();
		_prevAnimationFrameNo = frameNo;
	}

	XMVECTOR animatePosition = XMLoadFloat3(&_currentAnimationFrameIt->offset);
	if (_nextAnimationFrameIt != _motionKeys.end())
	{
		float t = static_cast<float>(frameNo - _currentAnimationFrameIt->frameNo) / static_cast<float>(_nextAnimationFrameIt->frameNo - _currentAnimationFrameIt->frameNo);

		t = GetYFromXOnBezier(t, _nextAnimationFrameIt->p1, _nextAnimationFrameIt->p2, 12);

		_animateRotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(_currentAnimationFrameIt->quaternion, _nextAnimationFrameIt->quaternion, t));
		XMStoreFloat3(&_animatePosition, XMVectorLerp(animatePosition, XMLoadFloat3(&_currentAnimationFrameIt->offset), t));
	}
	else
	{
		_animateRotation = XMMatrixRotationQuaternion(_currentAnimationFrameIt->quaternion);
	}
}

void BoneNode::AnimateIK(unsigned frameNo)
{
	if (_motionKeys.size() <= 0 || _ikSolver == nullptr)
	{
		return;
	}

	if (_prevIKFrameNo != frameNo)
	{
		_currentIKFrameIt = std::find_if(_ikKeys.rbegin(), _ikKeys.rend(),
			[frameNo](const VMDIKkey& key)
			{
				return key.frameNo <= frameNo;
			});

		_prevIKFrameNo = frameNo;
	}

	if (_currentIKFrameIt == _ikKeys.rend())
	{
		return;
	}

	_ikSolver->SetEnable(_currentIKFrameIt->enable);
}

float BoneNode::GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n)
{
	if (a.x == a.y && b.x == b.y)
	{
		return x;
	}

	float t = x;
	const float k0 = 1 + 3 * a.x - 3 * b.x;
	const float k1 = 3 * b.x - 6 * a.x;
	const float k2 = 3 * a.x;

	for (int i = 0; i < n; ++i)
	{
		auto ft = k0 * t * t * t + k1 * t * t + k2 * t - x;

		if (ft <= epsilon && ft >= -epsilon)
		{
			break;
		}

		t -= ft / 2;
	}

	auto r = 1 - t;
	return t * t * t + 3 * t * t * r * b.y + 3 * t * r * r * a.y;
}

