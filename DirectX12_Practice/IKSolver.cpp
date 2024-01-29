#include "IKSolver.h"
#include "MathUtil.h"

#include <math.h>

IKSolver::IKSolver(BoneNode* node, BoneNode* targetNode, unsigned iterationCount, float limitAngle) :
	_ikNode(node),
	_targetNode(targetNode),
	_ikIterationCount(iterationCount),
	_ikLimitAngle(limitAngle),
	_enable(true)
{
}

void IKSolver::Solve()
{
	if (_enable == false)
	{
		return;
	}

	if (_ikNode == nullptr || _targetNode == nullptr)
	{
		return;
	}

	for (IKChain& chain : _ikChains)
	{
		chain.prevAngle = DirectX::XMFLOAT3(0.f, 0.f, 0.f);
		chain.boneNode->SetIKRotation(XMMatrixIdentity());
		chain.planeModeAngle = 0.f;

		chain.boneNode->UpdateLocalTransform();
		chain.boneNode->UpdateGlobalTransform();
	}

	float maxDistance = std::numeric_limits<float>::max();
	for (unsigned int i = 0; i < _ikIterationCount; i++)
	{
		SolveCore(i);

		XMVECTOR targetPosition = _targetNode->GetGlobalTransform().r[3];
		XMVECTOR ikPosition = _ikNode->GetGlobalTransform().r[3];
		float dist = XMVector3Length(XMVectorSubtract(targetPosition, ikPosition)).m128_f32[0];

		if (dist < maxDistance)
		{
			maxDistance = dist;
			for (IKChain& chain : _ikChains)
			{
				XMStoreFloat4(&chain.saveIKRotation, XMQuaternionRotationMatrix(chain.boneNode->GetIKRotation()));
			}
		}
		else
		{
			for (IKChain& chain : _ikChains)
			{
				chain.boneNode->SetIKRotation(XMMatrixRotationQuaternion(XMLoadFloat4(&chain.saveIKRotation)));
				chain.boneNode->UpdateLocalTransform();
				chain.boneNode->UpdateGlobalTransform();
			}
			break;
		}
	}
}

const std::wstring& IKSolver::GetIKNodeName() const
{
	if (_ikNode == nullptr)
	{
		return L"";
	}
	else
	{
		return _ikNode->GetName();
	}
}

void IKSolver::SolveCore(unsigned iteration)
{
	XMVECTOR ikPosition = _ikNode->GetGlobalTransform().r[3];
	for (unsigned int chainIndex = 0; chainIndex < _ikChains.size(); chainIndex++)
	{
		IKChain& chain = _ikChains[chainIndex];
		BoneNode* chainNode = chain.boneNode;
		if (chainNode == nullptr)
		{
			continue;
		}

		if (chain.enableAxisLimit == true)
		{
			if ((chain.limitMin.x != 0 || chain.limitMax.x != 0) &&
				(chain.limitMin.y == 0 || chain.limitMax.y == 0) &&
				(chain.limitMin.z == 0 || chain.limitMax.z == 0))
			{
				SolvePlane(iteration, chainIndex, SolveAxis::X);
				continue;
			}
			else if ((chain.limitMin.y != 0 || chain.limitMax.y != 0) &&
				(chain.limitMin.x == 0 || chain.limitMax.x == 0) &&
				(chain.limitMin.z == 0 || chain.limitMax.z == 0))
			{
				SolvePlane(iteration, chainIndex, SolveAxis::Y);
				continue;
			}
			else if ((chain.limitMin.z != 0 || chain.limitMax.z != 0) &&
				(chain.limitMin.x == 0 || chain.limitMax.x == 0) &&
				(chain.limitMin.y == 0 || chain.limitMax.y == 0))
			{
				SolvePlane(iteration, chainIndex, SolveAxis::Z);
				continue;
			}
		}

		XMVECTOR targetPosition = _targetNode->GetGlobalTransform().r[3];

		XMVECTOR det = XMMatrixDeterminant(chain.boneNode->GetGlobalTransform());
		XMMATRIX inverseChain = XMMatrixInverse(&det, chain.boneNode->GetGlobalTransform());

		XMVECTOR chainIKPosition = XMVector3Transform(ikPosition, inverseChain);
		XMVECTOR chainTargetPosition = XMVector3Transform(targetPosition, inverseChain);

		XMVECTOR chainIKVector = XMVector3Normalize(chainIKPosition);
		XMVECTOR chainTargetVector = XMVector3Normalize(chainTargetPosition);

		float dot = XMVector3Dot(chainTargetVector, chainIKVector).m128_f32[0];
		dot = MathUtil::Clamp(dot, -1.f, 1.f);

		float angle = std::acos(dot);
		float angleDegree = XMConvertToDegrees(angle);
		if (angleDegree < 1.0e-3f)
		{
			continue;
		}

		angle = MathUtil::Clamp(angle, -_ikLimitAngle, _ikLimitAngle);
		XMVECTOR cross = XMVector3Normalize(XMVector3Cross(chainTargetVector, chainIKVector));
		XMMATRIX rotation = XMMatrixRotationAxis(cross, angle);

		XMMATRIX chainRotation = rotation * chainNode->GetAnimateRotation() * chainNode->GetIKRotation();
		if (chain.enableAxisLimit == true)
		{
			XMFLOAT3 rotXYZ = Decompose(chainRotation, chain.prevAngle);

			XMFLOAT3 clampXYZ = MathUtil::Clamp(rotXYZ, chain.limitMin, chain.limitMax);
			float invLimitAngle = -_ikLimitAngle;
			clampXYZ = MathUtil::Clamp(MathUtil::Sub(clampXYZ, chain.prevAngle), invLimitAngle, _ikLimitAngle);
			clampXYZ = MathUtil::Add(clampXYZ, chain.prevAngle);

			chainRotation = XMMatrixRotationRollPitchYaw(clampXYZ.x, clampXYZ.y, clampXYZ.z);
			chain.prevAngle = clampXYZ;
		}

		XMVECTOR det1 = XMMatrixDeterminant(chain.boneNode->GetAnimateRotation());
		XMMATRIX inverseAnimate = XMMatrixInverse(&det1, chain.boneNode->GetAnimateRotation());

		XMMATRIX ikRotation = inverseAnimate * chainRotation;
		chain.boneNode->SetIKRotation(ikRotation);

		chain.boneNode->UpdateLocalTransform();
		chain.boneNode->UpdateGlobalTransform();
	}
}

void IKSolver::SolvePlane(unsigned iteration, unsigned chainIndex, SolveAxis solveAxis)
{
	XMFLOAT3 rotateAxis;
	float limitMinAngle;
	float limitMaxAngle;

	IKChain& chain = _ikChains[chainIndex];

	switch (solveAxis)
	{
	case SolveAxis::X:
		limitMinAngle = chain.limitMin.x;
		limitMaxAngle = chain.limitMax.x;
		rotateAxis = XMFLOAT3(1.f, 0.f, 0.f);
		break;
	case SolveAxis::Y:
		limitMinAngle = chain.limitMin.y;
		limitMaxAngle = chain.limitMax.y;
		rotateAxis = XMFLOAT3(0.f, 1.f, 0.f);
		break;
	case SolveAxis::Z:
		limitMinAngle = chain.limitMin.z;
		limitMaxAngle = chain.limitMax.z;
		rotateAxis = XMFLOAT3(0.f, 0.f, 1.f);
		break;
	}

	XMVECTOR ikPosition = _ikNode->GetGlobalTransform().r[3];
	XMVECTOR targetPosision = _targetNode->GetGlobalTransform().r[3];

	XMVECTOR det = XMMatrixDeterminant(chain.boneNode->GetGlobalTransform());
	XMMATRIX inverseChain = XMMatrixInverse(&det, chain.boneNode->GetGlobalTransform());

	XMVECTOR chainIKPosition = XMVector3Transform(ikPosition, inverseChain);
	XMVECTOR chainTargetPosition = XMVector3Transform(targetPosision, inverseChain);

	XMVECTOR chainIKVector = XMVector3Normalize(chainIKPosition);
	XMVECTOR chainTargetVector = XMVector3Normalize(chainTargetPosition);

	float dot = XMVector3Dot(chainTargetVector, chainIKVector).m128_f32[0];
	dot = MathUtil::Clamp(dot, -1.f, 1.f);

	float angle = std::acos(dot);
	angle = MathUtil::Clamp(angle, -_ikLimitAngle, _ikLimitAngle);

	XMVECTOR rotation1 = XMQuaternionRotationAxis(XMLoadFloat3(&rotateAxis), angle);
	XMVECTOR targetVector1 = XMVector3Rotate(chainTargetVector, rotation1);
	float dot1 = XMVector3Dot(targetVector1, chainIKVector).m128_f32[0];

	XMVECTOR rotation2 = XMQuaternionRotationAxis(XMLoadFloat3(&rotateAxis), -angle);
	XMVECTOR targetVector2 = XMVector3Rotate(chainTargetVector, rotation2);
	float dot2 = XMVector3Dot(targetVector2, chainIKVector).m128_f32[0];

	float newAngle = chain.planeModeAngle;
	if (dot1 > dot2)
	{
		newAngle += angle;
	}
	else
	{
		newAngle -= angle;
	}

	if (iteration == 0)
	{
		if (newAngle < limitMinAngle || newAngle > limitMaxAngle)
		{
			if (-newAngle > limitMinAngle && -newAngle < limitMaxAngle)
			{
				newAngle *= -1;
			}
			else
			{
				float halfRadian = (limitMinAngle + limitMaxAngle) * 0.5f;
				if (abs(halfRadian - newAngle) > abs(halfRadian + newAngle))
				{
					newAngle *= -1;
				}
			}
		}
	}

	newAngle = MathUtil::Clamp(newAngle, limitMinAngle, limitMaxAngle);
	chain.planeModeAngle = newAngle;

	XMVECTOR det1 = XMMatrixDeterminant(chain.boneNode->GetAnimateRotation());
	XMMATRIX inverseAnimate = XMMatrixInverse(&det1, chain.boneNode->GetAnimateRotation());

	XMMATRIX ikRotation = inverseAnimate * XMMatrixRotationAxis(XMLoadFloat3(&rotateAxis), newAngle);

	chain.boneNode->SetIKRotation(ikRotation);

	chain.boneNode->UpdateLocalTransform();
	chain.boneNode->UpdateGlobalTransform();
}

XMFLOAT3 IKSolver::Decompose(const XMMATRIX& m, const XMFLOAT3& before)
{
	XMFLOAT3 r;
	float sy = -m.r[0].m128_f32[2];
	const float e = 1.0e-6f;

	if ((1.0f - std::abs(sy)) < e)
	{
		r.y = std::asin(sy);
		float sx = std::sin(before.x);
		float sz = std::sin(before.z);
		if (std::abs(sx) < std::abs(sz))
		{
			float cx = std::cos(before.x);
			if (cx > 0)
			{
				r.x = 0;
				r.z = std::asin(-m.r[1].m128_f32[0]);
			}
			else
			{
				r.x = XM_PI;
				r.z = std::asin(m.r[1].m128_f32[0]);
			}
		}
		else
		{
			float cz = std::cos(before.z);
			if (cz > 0)
			{
				r.z = 0;
				r.x = std::asin(-m.r[2].m128_f32[1]);
			}
			else
			{
				r.z = XM_PI;
				r.x = std::asin(m.r[2].m128_f32[1]);
			}
		}
	}
	else
	{
		r.x = std::atan2(m.r[1].m128_f32[2], m.r[2].m128_f32[2]);
		r.y = std::asin(-m.r[0].m128_f32[2]);
		r.z = std::atan2(m.r[0].m128_f32[1], m.r[0].m128_f32[0]);
	}

	const float pi = XM_PI;
	XMFLOAT3 tests[] =
	{
		{ r.x + pi, pi - r.y, r.z + pi },
		{ r.x + pi, pi - r.y, r.z - pi },
		{ r.x + pi, -pi - r.y, r.z + pi },
		{ r.x + pi, -pi - r.y, r.z - pi },
		{ r.x - pi, pi - r.y, r.z + pi },
		{ r.x - pi, pi - r.y, r.z - pi },
		{ r.x - pi, -pi - r.y, r.z + pi },
		{ r.x - pi, -pi - r.y, r.z - pi },
	};

	float errX = std::abs(DiffAngle(r.x, before.x));
	float errY = std::abs(DiffAngle(r.y, before.y));
	float errZ = std::abs(DiffAngle(r.z, before.z));
	float minErr = errX + errY + errZ;
	for (const auto test : tests)
	{
		float err = std::abs(DiffAngle(test.x, before.x))
			+ std::abs(DiffAngle(test.y, before.y))
			+ std::abs(DiffAngle(test.z, before.z));
		if (err < minErr)
		{
			minErr = err;
			r = test;
		}
	}
	return r;
}

float IKSolver::NormalizeAngle(float angle)
{
	float ret = angle;
	while (ret >= XM_2PI)
	{
		ret -= XM_2PI;
	}
	while (ret < 0)
	{
		ret += XM_2PI;
	}

	return ret;
}

float IKSolver::DiffAngle(float a, float b)
{
	float diff = NormalizeAngle(a) - NormalizeAngle(b);
	if (diff > XM_PI)
	{
		return diff - XM_2PI;
	}
	else if (diff < -XM_PI)
	{
		return diff + XM_2PI;
	}
	return diff;
}
