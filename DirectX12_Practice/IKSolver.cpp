#include "IKSolver.h"

void IKSolver::Solve()
{
	if (_enable == true)
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
		chain.planeModeAngle = 0.f;
	}

	float maxDistance = std::numeric_limits<float>::max();
	for (unsigned int i = 0; i < _ikIterationCount; i++)
	{
		SolveCore(i);
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
}
