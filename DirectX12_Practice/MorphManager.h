#pragma once
#include <DirectXMath.h>
#include <vector>
#include <unordered_map>

#include "Morph.h"

using namespace DirectX;

class MorphManager
{
public:
	MorphManager();

	void Init(const std::vector<PMXMorph>& pmxMorphs, const std::vector<VMDMorph>& vmdMorphs, unsigned int vertexCount);

	void Animate(unsigned int frame);

	const XMFLOAT3& GetMorphVertexPosition(unsigned int index) const;
	const XMFLOAT4& GetMorphUV(unsigned int index) const;

private:
	void AnimateMorph(Morph& morph);
	void AnimatePositionMorph(Morph& morph);
	void AnimateUVMorph(Morph& morph);

	void ResetMorphData();

	float Lerp(float a, float b, float t);

private:
	std::vector<Morph> _morphs;
	std::unordered_map<std::wstring, Morph*> _morphByName;

	std::vector<VMDMorph> _morphKeys;
	std::unordered_map<std::wstring, std::vector<VMDMorph*>> _morphKeyByName;

	std::vector<XMFLOAT3> _morphVertexPosition;
	std::vector<XMFLOAT4> _morphUV;
};

